#include "rigid_body.h"

#include <algorithm>
#include <queue>
#include <execution>

#include "scene.h"

namespace pmk
{
	std::vector<std::pair<float*, std::string>> RigidBodyModel::GetParameters()
	{
		return {};
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
	}

	void  RigidBodyContext::CreateRigidBodiesByConnectedness(renderer::VoxelChunk& voxel_chunk)
	{
		rigid_bodies_.clear();

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
	}

	// Returns true when the given coordinate is inside the region that flood fill is filling.
	bool FloodFillInside(const glm::uvec3& coord, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask)
	{
		// We only check upper condition since negative uints will overflow anyway.
		bool x_out_bounds{ coord.x >= CHUNK_ROW_VOXEL_COUNT };
		bool y_out_bounds{ coord.y >= CHUNK_ROW_VOXEL_COUNT };
		bool z_out_bounds{ coord.z >= CHUNK_ROW_VOXEL_COUNT };

		if (x_out_bounds || y_out_bounds || z_out_bounds) {
			return false;
		}

		uint32_t idx{ voxel_chunk.Coordinate(coord).physics_material_index };
		return idx == renderer::PHYSICS_MATERIAL_EMPTY_INDEX ? false : material_mask[idx];
	}

	renderer::Voxel FloodFillSet(uint32_t idx, renderer::VoxelChunk& voxel_chunk)
	{
		renderer::Voxel voxel{
			.physics_material_index = voxel_chunk.Index(idx).physics_material_index
		};
		voxel_chunk.Index(idx).physics_material_index = renderer::PHYSICS_MATERIAL_EMPTY_INDEX;
		return voxel;
	}

	void FloodFillScan(
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
		const float density{ (*physics_materials_)[physics_material_index]->constitutive_model->GetDensity() };
		return density * PARTICLE_VOLUME;
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

		RigidBody* rigid_body{ new RigidBody{
			.node = scene_->CreateNode(),
			.mass = mass,
			.center_of_mass = std::move(center_of_mass),
			.voxel_chunk = renderer::VoxelChunk(dimensions.x, dimensions.y, dimensions.z, std::move(voxel_pairs)),
		} };

		scene_->AddRenderObjectToNode(rigid_body->node, renderer_->CreateBlankRenderObject());
		renderer_->GenerateStaticParticleMesh(rigid_body->node->render_object, rigid_body->voxel_chunk);
		rigid_bodies_.push_back(rigid_body);
	}
}
