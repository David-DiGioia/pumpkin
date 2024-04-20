#include "rigid_body.h"

#include <algorithm>
#include <queue>
#include <execution>
#include <atomic>
#include "glm/gtc/quaternion.hpp"

#include "scene.h"

namespace pmk
{
	RigidBodyModel::RigidBodyModel()
	{
		density_ = 50.0f;
	}

	std::vector<std::pair<float*, std::string>> RigidBodyModel::GetParameters()
	{
		return { {&density_, "Density"} };
	}

	void RigidBodyModel::OnParametersMutated()
	{
		// No-op.
	}

	void RigidBodyContext::Initialize(renderer::VulkanRenderer* renderer, Scene* scene, const std::vector<PhysicsMaterial*>* physics_materials)
	{
		renderer_ = renderer;
		scene_ = scene;
		physics_materials_ = physics_materials;
	}

	void RigidBodyContext::CleanUp()
	{
		for (RigidBody* rb : rigid_bodies_) {
			delete rb;
		}
	}

	void RigidBodyContext::PhysicsUpdate(float delta_time)
	{
		if (!update_physics_) {
			return;
		}

		constexpr uint32_t substeps{ 8 };
		constexpr glm::vec3 gravity{ 0.0f, -9.81f, 0.0f };
		//constexpr glm::vec3 gravity{ 0.0f, 0.0f, 0.0f };

		// TODO: detect collision between all pairs of rigid bodies after doing large scale sweep.

		float h{ delta_time / substeps };
		for (uint32_t i{ 0 }; i < substeps; ++i)
		{
			for (RigidBody* rb : rigid_bodies_)
			{
				rb->previous_position = rb->node->position;
				rb->velocity = rb->immovable ? glm::vec3{} : rb->velocity + h * gravity;
				rb->node->position += h * rb->velocity;

				rb->previous_rotation = rb->node->rotation;
				rb->angular_velocity = rb->immovable ? glm::vec3{} : rb->angular_velocity + h * glm::inverse(rb->inertia_tensor) * (-glm::cross(rb->angular_velocity, rb->inertia_tensor * rb->angular_velocity));

				if (!rb->voxel_chunk.IsPointMass())
				{
					rb->node->rotation += h * 0.5f * glm::quat{ 0.0f, rb->angular_velocity.x, rb->angular_velocity.y, rb->angular_velocity.z } *rb->node->rotation;
					rb->node->rotation = glm::normalize(rb->node->rotation);
				}
			}

			SolvePositions(h);

			for (RigidBody* rb : rigid_bodies_)
			{
				rb->velocity = (rb->node->position - rb->previous_position) / h;
				if (!rb->voxel_chunk.IsPointMass())
				{
					glm::quat delta_q{ rb->node->rotation * glm::inverse(rb->previous_rotation) };
					rb->angular_velocity = 2.0f * glm::vec3{ delta_q.x, delta_q.y, delta_q.z } / h;
					if (delta_q.w < 0) {
						rb->angular_velocity = -rb->angular_velocity;
					}
				}
			}

			//SolveVelocities();
		}

#ifdef EDITOR_ENABLED
		if (generate_rb_voxel_instances_) {
			GenerateDynamicDebugRbVoxelInstances();
		}
#endif
	}

	void RigidBodyContext::EnablePhysicsUpdate()
	{
		update_physics_ = true;
	}

	void RigidBodyContext::DisablePhysicsUpdate()
	{
		update_physics_ = false;
	}

	bool RigidBodyContext::GetPhysicsUpdateEnabled() const
	{
		return update_physics_;
	}

	void RigidBodyContext::ResetRigidBodies()
	{
		DisablePhysicsUpdate();
		for (RigidBody* rb : rigid_bodies_) {
			scene_->DestroyNode(rb->node);
			delete rb;
		}
		rigid_bodies_.clear();
	}

	std::vector<uint32_t> RigidBodyContext::CreateRigidBodiesByConnectedness(renderer::VoxelChunk& voxel_chunk, bool* out_is_empty)
	{
		// Create a mask to quickly test if a static particle has a rigid body material.
		std::vector<uint8_t> rigid_body_mask(physics_materials_->size());
		std::transform(physics_materials_->begin(), physics_materials_->end(), rigid_body_mask.begin(),
			[](const pmk::PhysicsMaterial* m) { return dynamic_cast<const RigidBodyModel*>(m->constitutive_model) != nullptr; });

		for (uint32_t i{ 0 }; i < CHUNK_ROW_VOXEL_COUNT; ++i)
		{
			for (uint32_t j{ 0 }; j < CHUNK_ROW_VOXEL_COUNT; ++j)
			{
				for (uint32_t k{ 0 }; k < CHUNK_ROW_VOXEL_COUNT; ++k)
				{
					renderer::Voxel& p{ voxel_chunk.Coordinate(i, j, k) };

					if (p.physics_material_index == renderer::PHYSICS_MATERIAL_EMPTY_INDEX) {
						continue;
					}

#ifdef EDITOR_ENABLED
					// For editor convenience we just use available physics material if enough haven't been created yet.
					uint32_t idx{ std::min((uint32_t)p.physics_material_index, (uint32_t)(physics_materials_->size() - 1)) };
#else
					uint32_t idx{ (uint32_t)p.physics_material_index };
#endif

					if (rigid_body_mask[idx])
					{
						glm::vec3 center_of_mass{};
						RigidBodyFloodFill({ i, j, k }, voxel_chunk, rigid_body_mask);
					}
				}
			}
		}

		// Check for existence of non-rigid body voxels remaining.
		std::atomic_bool voxel_chunk_empty{ true };
		std::for_each(
			std::execution::par,
			voxel_chunk.GetVoxels().begin(),
			voxel_chunk.GetVoxels().end(),
			[&](renderer::Voxel v) {
				if (v.physics_material_index != renderer::PHYSICS_MATERIAL_EMPTY_INDEX) {
					voxel_chunk_empty.store(false, std::memory_order_relaxed);
				}
			});

		*out_is_empty = voxel_chunk_empty.load(std::memory_order_relaxed);

		std::vector<uint32_t> node_ids{};
		node_ids.resize(rigid_bodies_.size());
		std::transform(rigid_bodies_.begin(), rigid_bodies_.end(), node_ids.begin(),
			[](RigidBody* rb) { return rb->node->node_id; });
		return node_ids;
	}

	std::array<CollisionPair, MAX_COLLISION_PAIRS> RigidBodyContext::ComputeCollisionPairs(const RigidBody* a, const RigidBody* b, uint32_t* out_count) const
	{
		std::array<CollisionPair, MAX_COLLISION_PAIRS> collision_pairs{};
		const RigidBody* small{ a }; // Rigid body with fewer outer voxels.
		const RigidBody* big{ b };   // Rigid body with more outer voxels.

		bool ab_swap{ false };
		if (small->voxel_chunk.GetOuterVoxels().size() > big->voxel_chunk.GetOuterVoxels().size())
		{
			std::swap(small, big);
			ab_swap = true;
		}

		// TODO: Make this multithreaded.
		glm::mat4 small_world{ small->node->GetWorldTransform() };
		glm::mat4 inv_big_world{ glm::inverse(big->node->GetWorldTransform()) };
		uint32_t collision_pair_idx{ 0 };
		for (const renderer::OuterVoxel& ov : small->voxel_chunk.GetOuterVoxels())
		{
			glm::uvec3 small_coord{ ov.coord };
			glm::vec3 global_pos{ CoordinateToGlobal(small->center_of_mass, small_world, small_coord) };
			glm::uvec3 big_coord{ GetCollisionCoordinate(*big, inv_big_world, global_pos) };

			bool in_bounds{ big_coord.x != UINT_MAX };

			if (in_bounds && !big->voxel_chunk.IsEmpty(big_coord))
			{
				collision_pairs[collision_pair_idx++] = ab_swap ? CollisionPair{ big_coord, small_coord } : CollisionPair{ small_coord, big_coord };

				if (collision_pair_idx == MAX_COLLISION_PAIRS) {
					break;
				}
			}
		}

		*out_count = collision_pair_idx;
		return collision_pairs;
	}

	void RigidBodyContext::SetRigidBodyOverlayEnabled(bool enabled)
	{
		generate_rb_voxel_instances_ = enabled;

		if (enabled) {
			GenerateDynamicDebugRbVoxelInstances();
		}
	}

	void RigidBodyContext::SolvePositions(float h)
	{
		if (rigid_bodies_.empty()) {
			return;
		}

		constexpr float compliance{ 0.000001f };

		constexpr float alpha{ compliance };
		float alpha_tilde{ alpha / (h * h) };

		// TODO: Don't check every pair of rigid bodies.
		for (uint32_t a_idx{ 0 }; a_idx < (uint32_t)rigid_bodies_.size() - 1; ++a_idx)
		{
			for (uint32_t b_idx{ a_idx + 1 }; b_idx < (uint32_t)rigid_bodies_.size(); ++b_idx)
			{
				RigidBody* rb_a{ rigid_bodies_[a_idx] };
				RigidBody* rb_b{ rigid_bodies_[b_idx] };

				uint32_t count{};
				auto collision_pairs{ ComputeCollisionPairs(rb_a, rb_b, &count) };

				for (uint32_t i{ 0 }; i < count; ++i)
				{
					CollisionPair& cp{ collision_pairs[i] };
					// Local position of voxel centers.
					glm::vec3 r1_local{ ((glm::vec3)cp.coordinate_a - rb_a->center_of_mass) * PARTICLE_WIDTH };
					glm::vec3 r2_local{ ((glm::vec3)cp.coordinate_b - rb_b->center_of_mass) * PARTICLE_WIDTH };

					glm::vec3 world_center_of_mass_a{ rb_a->node->position };
					glm::vec3 world_center_of_mass_b{ rb_b->node->position };

					// World position of voxel centers.
					glm::vec3 world_pos_a{ glm::vec3{rb_a->node->GetWorldTransform() * glm::vec4{r1_local, 1.0f}} };
					glm::vec3 world_pos_b{ glm::vec3{rb_b->node->GetWorldTransform() * glm::vec4{r2_local, 1.0f}} };

					glm::vec3 delta_x{ world_pos_b - world_pos_a };
					float c{ glm::length(delta_x) };

					if (c > PARTICLE_WIDTH) {
						continue;
					}

					glm::vec3 n{ delta_x / c };
					c = std::fabsf(c - PARTICLE_WIDTH); // Distance between sphere surfaces instead of sphere centers.

					// Change world positions to be points on surface of sphere instead of center.
					world_pos_a += n * PARTICLE_RADIUS;
					world_pos_b -= n * PARTICLE_RADIUS;
					glm::vec3 r1{ world_pos_a - world_center_of_mass_a };
					glm::vec3 r2{ world_pos_b - world_center_of_mass_b };

					float m1{ rb_a->immovable ? std::numeric_limits<float>::infinity() : rb_a->mass };
					float m2{ rb_b->immovable ? std::numeric_limits<float>::infinity() : rb_b->mass };
					glm::vec3 r1_cross_n{ glm::cross(r1, n) };
					glm::vec3 r2_cross_n{ glm::cross(r2, n) };
					glm::mat3 inertia_tensor_inv_a{ rb_a->immovable || rb_a->voxel_chunk.IsPointMass() ? glm::mat3{} : glm::inverse(rb_a->inertia_tensor) };
					glm::mat3 inertia_tensor_inv_b{ rb_b->immovable || rb_b->voxel_chunk.IsPointMass() ? glm::mat3{} : glm::inverse(rb_b->inertia_tensor) };
					float w1{ (1.0f / m1) + glm::dot(r1_cross_n, inertia_tensor_inv_a * r1_cross_n) };
					float w2{ (1.0f / m2) + glm::dot(r2_cross_n, inertia_tensor_inv_b * r2_cross_n) };

					float lambda{ 0.0f };
					float delta_lambda{ (-c - alpha_tilde * lambda) / (w1 + w2 + alpha_tilde) };
					lambda += delta_lambda;

					glm::vec3 p{ delta_lambda * n };

					if (!rb_a->immovable)
					{
						rb_a->node->position += p / m1;
						if (!rb_a->voxel_chunk.IsPointMass())
						{
							glm::vec3 tmp1{ inertia_tensor_inv_a * glm::cross(r1, p) };
							rb_a->node->rotation += 0.5f * glm::quat{ 0.0f, tmp1.x, tmp1.y, tmp1.z } *rb_a->node->rotation;
						}
					}

					if (!rb_b->immovable)
					{
						rb_b->node->position -= p / m2;
						if (!rb_b->voxel_chunk.IsPointMass())
						{
							glm::vec3 tmp2{ inertia_tensor_inv_b * glm::cross(r2, p) };
							rb_b->node->rotation -= 0.5f * glm::quat{ 0.0f, tmp2.x, tmp2.y, tmp2.z } *rb_b->node->rotation;
						}
					}
				}
			}
		}
	}

	// Returns true when the given coordinate is inside the region that flood fill is filling.
	static bool FloodFillInside(const glm::uvec3& coord, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask)
	{
		// We only check upper condition since negative uints will overflow anyway.
		bool x_out_bounds{ coord.x >= CHUNK_ROW_VOXEL_COUNT };
		bool y_out_bounds{ coord.y >= CHUNK_ROW_VOXEL_COUNT };
		bool z_out_bounds{ coord.z >= CHUNK_ROW_VOXEL_COUNT };

		if (x_out_bounds || y_out_bounds || z_out_bounds) {
			return false;
		}

		uint32_t idx{ voxel_chunk.Coordinate(coord).physics_material_index };

#ifdef EDITOR_ENABLED
		// For editor convenience we just use available physics material if enough haven't been created yet.
		if (idx != renderer::PHYSICS_MATERIAL_EMPTY_INDEX) {
			idx = std::min(idx, (uint32_t)material_mask.size() - 1);
		}
#endif

		return idx == renderer::PHYSICS_MATERIAL_EMPTY_INDEX ? false : material_mask[idx];
	}

	static renderer::Voxel FloodFillSet(uint32_t idx, renderer::VoxelChunk& voxel_chunk)
	{
		renderer::Voxel voxel{
			.physics_material_index = voxel_chunk.Index(idx).physics_material_index
		};
		voxel_chunk.Index(idx).physics_material_index = renderer::PHYSICS_MATERIAL_EMPTY_INDEX;
		return voxel;
	}

	static void FloodFillScan(
		uint32_t lx,
		uint32_t rx,
		uint32_t y,
		uint32_t z,
		std::queue<glm::uvec3>& queue,
		renderer::VoxelChunk& voxel_chunk,
		const std::vector<uint8_t>& material_mask)
	{
		bool span_added{ false };

		for (uint32_t x{ lx }; x < rx; ++x)
		{
			if (!FloodFillInside({ x, y, z }, voxel_chunk, material_mask)) {
				span_added = false;
			}
			else if (!span_added)
			{
				queue.push({ x, y, z });
				span_added = true;
			}
		}
	}

	// Creates rigid body based on connected voxels, and adds to rigid_bodies_.
	void RigidBodyContext::RigidBodyFloodFill(const glm::uvec3& coordinate, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask)
	{
		std::vector<std::pair<renderer::Voxel, glm::uvec3>> voxel_pairs{};
		glm::uvec3 min_extents{ UINT_MAX, UINT_MAX, UINT_MAX };
		glm::uvec3 max_extents{};
		glm::vec3 center_of_mass{};
		glm::mat3 intertia_tensor{};
		float rigid_body_mass{};
		std::queue<glm::uvec3> queue{};
		queue.push(coordinate);

		while (!queue.empty())
		{
			glm::uvec3 coord{ queue.front() };
			queue.pop();
			uint32_t lx{ coord.x };

			// Update extents.
			min_extents.x = std::min(min_extents.x, coord.x);
			min_extents.y = std::min(min_extents.y, coord.y);
			min_extents.z = std::min(min_extents.z, coord.z);
			max_extents.x = std::max(max_extents.x, coord.x);
			max_extents.y = std::max(max_extents.y, coord.y);
			max_extents.z = std::max(max_extents.z, coord.z);

			glm::uvec3 next_coord{ lx - 1, coord.y, coord.z };
			while (FloodFillInside(next_coord, voxel_chunk, material_mask))
			{
				voxel_pairs.push_back({ FloodFillSet(voxel_chunk.CoordinateToIndex(next_coord), voxel_chunk), next_coord });
				const float voxel_mass{ GetVoxelMass(voxel_pairs.back().first.physics_material_index) };
				rigid_body_mass += voxel_mass;
				center_of_mass += voxel_mass * glm::vec3{ next_coord };

				// Update x extents.
				min_extents.x = std::min(min_extents.x, coord.x);

				--lx;
				next_coord = { lx - 1, coord.y, coord.z };
			}

			while (FloodFillInside(coord, voxel_chunk, material_mask))
			{
				voxel_pairs.push_back({ FloodFillSet(voxel_chunk.CoordinateToIndex(coord), voxel_chunk), coord });
				const float voxel_mass{ GetVoxelMass(voxel_pairs.back().first.physics_material_index) };
				rigid_body_mass += voxel_mass;
				center_of_mass += voxel_mass * glm::vec3{ coord };

				// Update x extents.
				max_extents.x = std::max(max_extents.x, coord.x);

				++coord.x;
			}

			FloodFillScan(lx, coord.x, coord.y + 1, coord.z, queue, voxel_chunk, material_mask);
			FloodFillScan(lx, coord.x, coord.y - 1, coord.z, queue, voxel_chunk, material_mask);
			FloodFillScan(lx, coord.x, coord.y, coord.z + 1, queue, voxel_chunk, material_mask);
			FloodFillScan(lx, coord.x, coord.y, coord.z - 1, queue, voxel_chunk, material_mask);
		}

		center_of_mass /= rigid_body_mass;
		CreateRigidBody(min_extents, max_extents, std::move(voxel_pairs), std::move(center_of_mass), rigid_body_mass);
	}

	float RigidBodyContext::GetVoxelMass(uint32_t physics_material_index) const
	{
#ifdef EDITOR_ENABLED
		// For editor convenience we just use available physics material if enough haven't been created yet.
		physics_material_index = std::min(physics_material_index, (uint32_t)physics_materials_->size() - 1);
#endif
		const float density{ (*physics_materials_)[physics_material_index]->constitutive_model->GetDensity() };
		return density * PARTICLE_VOLUME;
	}

	glm::vec3 RigidBodyContext::CoordinateToGlobal(const glm::vec3& center_of_mass, const glm::mat4& world_transform, const glm::uvec3& voxel_coord) const
	{
		glm::vec3 local_space{ PARTICLE_WIDTH * glm::vec3{voxel_coord} };
		local_space -= center_of_mass * PARTICLE_WIDTH;
		glm::vec4 world_space{ world_transform * glm::vec4{ local_space, 1.0f} };

		return glm::vec3{ world_space };
	}

	glm::uvec3 RigidBodyContext::GetCollisionCoordinate(const RigidBody& rb, const glm::mat4& inv_world_transform, const glm::vec3& global_pos) const
	{
		glm::vec3 coord_space{ glm::vec3{inv_world_transform * glm::vec4{global_pos, 1.0f}} / PARTICLE_WIDTH };
		coord_space += rb.center_of_mass;

		uint32_t potential_collision_count{};
		std::array<glm::uvec3, 8> potential_collisions{ rb.voxel_chunk.GetPotentialCollisions(coord_space, &potential_collision_count) };

		float closest_sqr{ std::numeric_limits<float>::infinity() };
		float closest_idx{};
		for (uint32_t i{ 0 }; i < potential_collision_count; ++i)
		{
			float distance_sqr{ glm::distance2(coord_space, glm::vec3{ potential_collisions[i] }) };
			if (distance_sqr < closest_sqr)
			{
				closest_sqr = distance_sqr;
				closest_idx = i;
			}
		}

		// Check if it's less than one since we are coord space, where 1 voxel width is 1 unit.
		if (closest_sqr < 1.0f) {
			return potential_collisions[closest_idx];
		}

		return glm::uvec3{ UINT_MAX };
	}

	glm::mat3 RigidBodyContext::ComputeInertiaTensor(const renderer::VoxelChunk& voxel_chunk, const glm::vec3& center_of_mass)
	{
		float xx{};
		float yy{};
		float zz{};
		float xy{};
		float yz{};
		float xz{};

		for (uint32_t i{ 0 }; i < voxel_chunk.GetWidth(); ++i)
		{
			for (uint32_t j{ 0 }; j < voxel_chunk.GetHeight(); ++j)
			{
				for (uint32_t k{ 0 }; k < voxel_chunk.GetDepth(); ++k)
				{
					uint8_t physics_mat_index{ voxel_chunk.Coordinate(i, j, k).physics_material_index };
					if (physics_mat_index == renderer::PHYSICS_MATERIAL_EMPTY_INDEX) {
						continue;
					}

					glm::vec3 r{ i, j, k };
					glm::vec3 delta_r{ (r - center_of_mass) * PARTICLE_WIDTH };

					// The square of the cross product matrix of deltra_r.
					float x2{ delta_r.x * delta_r.x };
					float y2{ delta_r.y * delta_r.y };
					float z2{ delta_r.z * delta_r.z };
					float x_times_y{ delta_r.x * delta_r.y };
					float x_times_z{ delta_r.x * delta_r.z };
					float y_times_z{ delta_r.y * delta_r.z };
					float mass{ GetVoxelMass(physics_mat_index) };

					xx += (y2 + z2) * mass;
					yy += (x2 + z2) * mass;
					zz += (x2 + y2) * mass;
					xy -= x_times_y * mass;
					yz -= y_times_z * mass;
					xz -= x_times_z * mass;
				}
			}
		}

		return glm::mat3{
			xx, xy, xz,
			xy, yy, yz,
			xz, yz, zz,
		};
	}

	void RigidBodyContext::CreateRigidBody(
		const glm::uvec3& min_extents,
		const glm::uvec3& max_extents,
		std::vector<std::pair<renderer::Voxel, glm::uvec3>>&& voxel_pairs,
		glm::vec3&& center_of_mass,
		float mass)
	{
		// Subtract min extents to make all coordinates relative to it.
		std::for_each(
			std::execution::par,
			voxel_pairs.begin(),
			voxel_pairs.end(),
			[&](std::pair<renderer::Voxel, glm::uvec3>& pair)
			{
				pair.second -= min_extents;
			});
		center_of_mass -= min_extents;
		glm::uvec3 dimensions{ max_extents - min_extents + glm::uvec3{1, 1, 1} };

		auto voxel_chunk{ renderer::VoxelChunk(dimensions.x, dimensions.y, dimensions.z, std::move(voxel_pairs)) };
		RigidBody* rigid_body{ new RigidBody{
			.node = scene_->CreateNode(),
			.mass = mass,
			.center_of_mass = center_of_mass,
			.inertia_tensor = ComputeInertiaTensor(voxel_chunk, center_of_mass),
			.voxel_chunk = std::move(voxel_chunk),
		} };

		rigid_body->node->rigid_body = rigid_body;
		rigid_body->node->SetWorldPosition(PARTICLE_WIDTH * (glm::vec3{ min_extents } + rigid_body->center_of_mass));
		scene_->AddRenderObjectToNode(rigid_body->node, renderer_->CreateBlankRenderObject());
		renderer_->GenerateStaticParticleMesh(rigid_body->node->render_object, rigid_body->voxel_chunk, PARTICLE_WIDTH * rigid_body->center_of_mass);
		rigid_bodies_.push_back(rigid_body);
	}

	void RigidBodyContext::GenerateDynamicDebugRbVoxelInstances() const
	{
		size_t outer_voxel_count{};
		for (const RigidBody* rb : rigid_bodies_) {
			outer_voxel_count += rb->voxel_chunk.GetOuterVoxels().size();
		}

		if (outer_voxel_count == 0) {
			return;
		}

		std::vector<renderer::RigidBodyDebugVoxelInstance> debug_instances{};
		debug_instances.reserve(outer_voxel_count);
		for (const RigidBody* rb : rigid_bodies_)
		{
			glm::mat4 world_transform{ rb->node->GetWorldTransform() };
			glm::mat3 rotation{ glm::toMat3(rb->node->rotation) };
			for (const renderer::OuterVoxel& ov : rb->voxel_chunk.GetOuterVoxels())
			{
				renderer::RigidBodyDebugVoxelInstance debug_instance{
					.position = CoordinateToGlobal(rb->center_of_mass, world_transform, ov.coord),
					.normal = rotation * ov.normal,
				};

				debug_instances.push_back(std::move(debug_instance));
			}
		}

		renderer_->SetDebugRbVoxelInstances(debug_instances);
	}
}
