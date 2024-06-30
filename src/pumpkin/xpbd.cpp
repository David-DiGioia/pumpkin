#include "xpbd.h"

#include <cmath>
#include <bit>
#include <execution>
#include <ranges>
#include <thread>
#include "tracy/Tracy.hpp"
#include "glm/gtx/norm.hpp"

#include "common_constants.h"
#include "physics.h"
#include "rigid_body.h"
#include "scene.h"

namespace pmk
{
	constexpr uint32_t HASH_TABLE_SIZE{ 64000 };
	constexpr float GRID_SPACING{ PARTICLE_WIDTH };
	constexpr float SPH_KERNEL_RADIUS{ GRID_SPACING };
	constexpr float SPH_KERNEL_RADIUS_SQUARED{ SPH_KERNEL_RADIUS * SPH_KERNEL_RADIUS };

	// Only the first 21 bits of each input are used.
	static uint64_t InterleaveBits(glm::uvec3 c)
	{
		c.x = (c.x | (c.x << 16)) & 0xFF0000FF;
		c.x = (c.x | (c.x << 8)) & 0x0F00F00F;
		c.x = (c.x | (c.x << 4)) & 0xC30C30C3;
		c.x = (c.x | (c.x << 2)) & 0x49249249;

		c.y = (c.y | (c.y << 16)) & 0xFF0000FF;
		c.y = (c.y | (c.y << 8)) & 0x0F00F00F;
		c.y = (c.y | (c.y << 4)) & 0xC30C30C3;
		c.y = (c.y | (c.y << 2)) & 0x49249249;

		c.z = (c.z | (c.z << 16)) & 0xFF0000FF;
		c.z = (c.z | (c.z << 8)) & 0x0F00F00F;
		c.z = (c.z | (c.z << 4)) & 0xC30C30C3;
		c.z = (c.z | (c.z << 2)) & 0x49249249;

		return c.x | ((uint64_t)c.y << 1) | ((uint64_t)c.z << 2);
	}

	static glm::uvec3 PositionToCoordinate(const glm::vec3& position)
	{
		// Positive only coordinates, so we offset coordinates so that they are valid in every direction of origin.
		constexpr int32_t coord_offset{ 1000000 };

		return glm::uvec3{
			(uint32_t)((int32_t)std::floorf(position.x / GRID_SPACING) + 1000000),
			(uint32_t)((int32_t)std::floorf(position.y / GRID_SPACING) + 1000000),
			(uint32_t)((int32_t)std::floorf(position.z / GRID_SPACING) + 1000000),
		};
	}

	static uint32_t HashCoords(const glm::uvec3& coord)
	{
		return InterleaveBits(coord) % HASH_TABLE_SIZE;
	}

	static uint32_t HashPosition(const glm::vec3& pos)
	{
		return HashCoords(PositionToCoordinate(pos));
	}

	// TODO: Maybe replace kernel and gradient with lookup table.
	// From https://pysph.readthedocs.io/en/latest/reference/kernels.html.
	static float SPHKernel(float q)
	{
		constexpr float h{ SPH_KERNEL_RADIUS / 2.0f };
		constexpr float sigma_3{ 1.0f / (PI * h * h * h) };
		q /= h;

		if (q <= 1.0f) {
			return sigma_3 * (1.0f - 1.5f * q * q * (1.0f - 0.5f * q));
		}
		else if (q <= 2.0f)
		{
			float a{ (2.0f - q) };
			return 0.25f * sigma_3 * a * a * a;
		}

		return 0.0f;
	}

	// Calculated using Wolfram Alpha.
	static glm::vec3 SPHKernelGradient(glm::vec3 q)
	{
		constexpr float h{ SPH_KERNEL_RADIUS / 2.0f };
		constexpr float sigma_3{ 1.0f / (PI * h * h * h) };
		q /= h;

		float q_length{ glm::length(q) };

		if (q_length != 0.0f)
		{
			glm::vec3 n{ q / q_length };

			if (q_length <= 1.0f) {
				return n * ((sigma_3 * q_length * (2.25f * q_length - 3.0f)) / h);
			}
			else if (q_length <= 2.0f)
			{
				float a{ q_length - 2.0f };
				return n * ((-0.75f * sigma_3 * a * a) / h);
			}
		}

		return glm::vec3{ 0.0f, 0.0f, 0.0f };
	}

	void XPBDParticleContext::Initialize(
		std::vector<XPBDParticle>&& particles,
		float chunk_width,
		const std::vector<XPBDConstraint*>* jacobi_constraints,
		const std::vector<PhysicsMaterial*>* physics_materials)
	{
		particles_ = std::move(particles);
		jacobi_constraints_ = jacobi_constraints;
		physics_materials_ = physics_materials;
		rb_collisions_.resize(particles_.size());
		hash_table_.clear();
		hash_table_.resize(HASH_TABLE_SIZE, NULL_INDEX);

		// Create cache optimal particles.
		particles_stripped_ = static_cast<XPBDParticleStripped*>(::operator new[](particles_.size() * sizeof(XPBDParticleStripped), std::align_val_t{ CL_SIZE }));

		float particle_width = chunk_width / CHUNK_ROW_VOXEL_COUNT;
		particle_radius_ = 0.5f * particle_width;
		particle_initial_volume_ = particle_width * particle_width * particle_width;

		// Initialize particle mass and index buffer.
		for (XPBDParticle& p : particles_)
		{
			p.s.inverse_mass = 1.0f / (GetPhysicsMaterial(p)->density * particle_initial_volume_);
			p.key = HashPosition(p.position);
		}

		UpdateIndexBuffers();
	}

	void XPBDParticleContext::CleanUp()
	{
		::operator delete[](particles_stripped_, std::align_val_t{ CL_SIZE });
	}

	void XPBDParticleContext::SimulateStep(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		ZoneScoped;

		constexpr uint32_t iterations{ 3 };

		// Zero out rigid body-particle collisions.
		std::memset(rb_collisions_.data(), 0, rb_collisions_.size() * sizeof(RigidBodyParticleCollisionInfo));

		ApplyForces(delta_time);
		{
			ZoneScopedN("Solve all constraints");
			for (uint32_t i{ 0 }; i < iterations; ++i) {
				SolveConstraints(delta_time, rb_context);
			}
		}
		UpdateVelocityAndInternalForces(delta_time);

		UpdateIndexBuffers();
	}

	const std::vector<XPBDParticle>& XPBDParticleContext::GetParticles() const
	{
		return particles_;
	}

	std::vector<XPBDParticle>& XPBDParticleContext::GetParticles()
	{
		return particles_;
	}

	RigidBodyParticleCollisionInfo& XPBDParticleContext::GetRigidBodyCollision(uint32_t particle_idx)
	{
		return rb_collisions_[particle_idx];
	}

	void XPBDParticleContext::ApplyForces(float delta_time)
	{
		ZoneScoped;
		for (XPBDParticle& p : particles_)
		{
			// Apply forces.
			p.velocity += delta_time * glm::vec3{ 0.0f, -9.8f, 0.0f }; // Gravity.

			// Predict position.
			p.s.predicted_position = p.position + delta_time * p.velocity;
		}
	}

	void XPBDParticleContext::SolveConstraints(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		ZoneScoped;

		{
			ZoneScopedN("Preprocess");
			// Preprocess each constraint.
			for (XPBDConstraint* constraint : *jacobi_constraints_) {
				constraint->Preprocess(this, rb_context, delta_time);
			}
		}

		{
			ZoneScopedN("Copy to stripped particles");
			for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i) {
				std::memcpy(&particles_stripped_[i], &particles_[i].s, sizeof(XPBDParticleStripped));
			}
		}

		{
			ZoneScopedN("Parallel solve collisions");
			// Jacobi iterations.
			constexpr uint32_t chunk_size{ 64 };
			const uint32_t chunk_count{ ((uint32_t)particles_.size() + chunk_size - 1) / chunk_size }; // Round up.
			auto chunk_indices{ std::views::iota(0u, chunk_count) };
			std::for_each(std::execution::par, chunk_indices.begin(), chunk_indices.end(),
				[&](uint32_t chunk_idx) {
					uint32_t begin{ chunk_idx * chunk_size };
					uint32_t end{ std::min(begin + chunk_size, (uint32_t)particles_.size()) };
					for (uint32_t i{ begin }; i < end; ++i)
					{
						PhysicsMaterial* mat{ GetPhysicsMaterial(particles_[i]) };

						for (uint32_t j{ 0 }; j < (uint32_t)jacobi_constraints_->size(); ++j)
						{
							if (mat->jacobi_constraints_mask & (1 << j)) {
								particles_[i].s.predicted_position += (*jacobi_constraints_)[j]->Solve(this, rb_context, i, delta_time);
							}
						}
					}
				});
		}
	}

	void XPBDParticleContext::UpdateVelocityAndInternalForces(float delta_time)
	{
		ZoneScoped;

		auto indices{ std::views::iota(0u, (uint32_t)particles_.size()) };
		std::for_each(std::execution::par, indices.begin(), indices.end(),
			[&](uint32_t i) {
				XPBDParticle& p{ particles_[i] };

				// Update velocity.
				p.velocity = (p.s.predicted_position - p.position) / delta_time;
				p.key = HashPosition(p.s.predicted_position);
				p.position = p.s.predicted_position;

				// In the future, internal forces like drag and vorticity will be applied here.
			});
	}

	float XPBDParticleContext::ComputeDensity(const glm::vec3& pos, const ConstIndexProximityContainer& proximity_particles) const
	{
		float density{ 0.0f };

		uint32_t i{ 0 };
		//for (uint32_t j{ 0 }; j < (uint32_t)particles_.size(); ++j)
		for (uint32_t j : proximity_particles)
		{
			++i;
			float delta_pos{ glm::length(pos - particles_stripped_[j].predicted_position) };
			if (delta_pos < SPH_KERNEL_RADIUS) {
				density += (1.0f / particles_stripped_[j].inverse_mass) * SPHKernel(delta_pos);
			}
		}

		return density;
	}

	XPBDParticleContext::ProximityContainer XPBDParticleContext::GetParticlesByProximity(const glm::vec3& position)
	{
		return ProximityContainer(this, position);
	}

	XPBDParticleContext::ConstProximityContainer XPBDParticleContext::GetParticlesByProximity(const glm::vec3& position) const
	{
		return ConstProximityContainer(this, position);
	}

	XPBDParticleContext::IndexProximityContainer XPBDParticleContext::GetParticleIndicesByProximity(const glm::vec3& position)
	{
		return IndexProximityContainer(this, position);
	}

	XPBDParticleContext::ConstIndexProximityContainer XPBDParticleContext::GetParticleIndicesByProximity(const glm::vec3& position) const
	{
		return ConstIndexProximityContainer(this, position);
	}

	std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> XPBDParticleContext::GetParticleRangesWithinKernel(const glm::vec3& position, uint32_t* out_block_count) const
	{
		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> result{};
		glm::uvec3 coord{ PositionToCoordinate(position) };

		uint32_t result_idx{ 0 };
		for (uint32_t i{ coord.x - 1 }; i <= coord.x + 1; ++i)
		{
			for (uint32_t j{ coord.y - 1 }; j <= coord.y + 1; ++j)
			{
				for (uint32_t k{ coord.z - 1 }; k <= coord.z + 1; ++k)
				{
					glm::uvec3 neighbor_coord{ i, j, k };
					uint32_t particle_idx{ hash_table_[HashCoords(neighbor_coord)] };
					if (particle_idx != NULL_INDEX) {
						result[result_idx++] = particle_idx;
					}
				}
			}
		}

		*out_block_count = result_idx;
		return result;
	}


	void XPBDParticleContext::UpdateIndexBuffers()
	{
		ZoneScoped;
		{
			ZoneScopedN("Sort particles");
			std::sort(particles_.begin(), particles_.end(),
				[](const XPBDParticle& p0, const XPBDParticle& p1) { return p0.key < p1.key; });
		}

		std::memset(hash_table_.data(), NULL_INDEX, HASH_TABLE_SIZE * sizeof(uint32_t));

		{
			ZoneScopedN("Update hash table");
			uint32_t current_key{ NULL_INDEX };
			for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
			{
				XPBDParticle& p{ particles_[i] };
				if ((p.key != current_key) && (p.key != NULL_INDEX))
				{
					hash_table_[p.key] = i;
					current_key = p.key;
				}
			}
		}
	}

	const PhysicsMaterial* XPBDParticleContext::GetPhysicsMaterial(const XPBDParticle& p) const
	{
		return (*physics_materials_)[p.physics_material_index];
	}

	PhysicsMaterial* XPBDParticleContext::GetPhysicsMaterial(const XPBDParticle& p)
	{
		return (*physics_materials_)[p.physics_material_index];
	}

	void FluidDensityConstraint::Preprocess(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, float delta_time)
	{
		lambda_cache_.resize(p_context->GetParticles().size());
		const std::vector<XPBDParticle>& particles{ p_context->GetParticles() };

		// Compute lambda corresponding to each particle. It will be used in Solve().
		for (uint32_t i{ 0 }; i < (uint32_t)particles.size(); ++i)
		{
			const glm::vec3& position{ particles[i].s.predicted_position };

			constexpr float compliance{ 0.0f };
			constexpr float alpha{ compliance };
			float alpha_tilde{ alpha / (delta_time * delta_time) };

			auto proximity_particles{ p_context->GetParticleIndicesByProximity(position) };
			// TODO: Add rigid body density.
			float density{ p_context->ComputeDensity(position, proximity_particles) };
			float c{ (density / rest_density_) - 1.0f };

			// Clamp pressure constraint to be nonnegative.
			if (c <= 0.0f)
			{
				lambda_cache_[i] = 0.0f;
				return;
			}

			float denominator{ 0.0f };
			glm::vec3 k_equal_i_grad{};
			for (uint32_t k : proximity_particles)
			{
				if (k == i)
				{
					// From equation (8) of PBF, it's a sum over all neighbors. But we accumulate it separately with k_equal_i_grad.
					continue;
				}

				glm::vec3 q{ position - particles[k].s.predicted_position };
				if (glm::length2(q) >= SPH_KERNEL_RADIUS_SQUARED) {
					continue;
				}

				glm::vec3 grad_pk_w{ SPHKernelGradient(q) };
				denominator += glm::length2((-1.0f / rest_density_) * grad_pk_w); // Equation (8) case k=j from PBF.

				k_equal_i_grad += grad_pk_w;
			}
			denominator += glm::length2((1.0f / rest_density_) * k_equal_i_grad); // Equation (8) case k=i from PBF.

			lambda_cache_[i] = -c / (denominator + alpha_tilde); // Equation (9) from PBF.
		}
	}


	glm::vec3 FluidDensityConstraint::Solve(XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const
	{
		const std::vector<XPBDParticle>& particles{ p_context->GetParticles() };
		const XPBDParticle& particle{ particles[particle_idx] };
		const glm::vec3& position{ particle.s.predicted_position };

		glm::vec3 delta_x{};
		for (uint32_t j : p_context->GetParticleIndicesByProximity(position))
		{
			if (particle_idx == j) {
				continue;
			}

			glm::vec3 q{ position - particles[j].s.predicted_position };
			delta_x += (lambda_cache_[particle_idx] + lambda_cache_[j]) * SPHKernelGradient(q); // Equation (12) from PBF.
		}
		delta_x /= rest_density_;

		return delta_x;
	}

	std::vector<std::pair<float*, std::string>> FluidDensityConstraint::GetParameters()
	{
		return {
			{&rest_density_, "Rest density"},
		};
	}

	void FluidDensityConstraint::OnParametersMutated()
	{
		// No-op.
	}

	void CollisionConstraint::Preprocess(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, float delta_time)
	{
		// No-op.
	}

	glm::vec3 CollisionConstraint::Solve(XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const
	{
		const std::vector<XPBDParticle>& particles{ p_context->GetParticles() };

		float compliance_term{ compliance_ / (delta_time * delta_time) };

		const XPBDParticle& p1{ p_context->GetParticles()[particle_idx] };
		glm::vec3 particle_delta_x{};

		// Detect particle collisions.
		for (uint32_t p2_idx : p_context->GetParticleIndicesByProximity(p1.s.predicted_position))
		{
			if (p2_idx == particle_idx) {
				continue;
			}

			const XPBDParticle& p2{ p_context->GetParticles()[p2_idx] };
			glm::vec3 diff{ p1.s.predicted_position - p2.s.predicted_position };
			float distance2{ glm::length2(diff) };

			if (distance2 == 0.0f) {
				diff = glm::vec3{ 0.0f, 0.0001f, 0.0f };
				distance2 = glm::length2(diff);
			}

			if (distance2 >= PARTICLE_WIDTH_SQUARED) {
				continue;
			}

			float distance{ std::sqrtf(distance2) };
			float c{ distance - PARTICLE_WIDTH };
			glm::vec3 delta_c1{ diff / distance };

			float lambda{ -c / (p1.s.inverse_mass + p2.s.inverse_mass + compliance_term) }; // Magnitude of gradients are 1.0, so they're not written here.
			particle_delta_x += lambda * p1.s.inverse_mass * delta_c1;
		}

		// TODO: Don't iterate over all rigid bodies.
		// Detect rigid body collisions.
		RigidBodyParticleCollisionInfo& rb_collision{ p_context->GetRigidBodyCollision(particle_idx) };
		rb_collision.rb_index = NULL_INDEX;
		uint32_t rb_idx{ 0 };
		for (const RigidBody* rb : rb_context->GetRigidBodies())
		{
			std::optional<glm::vec3> rb_voxel_pos{ rb_context->ComputeParticleCollision(rb, p1.s.predicted_position) };

			if (rb_voxel_pos.has_value())
			{
				glm::vec3 diff{ p1.s.predicted_position - rb_voxel_pos.value() };
				float distance{ glm::length(diff) };
				float c{ distance - PARTICLE_WIDTH };
				glm::vec3 grad_c{ diff / distance };


				// Rigid body update that will be applied during rigid body physics update.
				glm::vec3& n{ grad_c };
				glm::vec3 world_pos_b = rb_voxel_pos.value() - n * PARTICLE_RADIUS;
				glm::vec3 r{ rb_voxel_pos.value() - rb->node->position };
				float rb_inv_mass{ rb->immovable ? 0.0f : (1.0f / rb->mass) };
				glm::vec3 r_cross_n{ glm::cross(r, n) };
				glm::mat3 inertia_tensor_inv_b{ rb->immovable || rb->voxel_chunk.IsPointMass() ? glm::mat3{} : glm::inverse(rb->inertia_tensor) };
				float rb_weight{ rb_inv_mass + glm::dot(r_cross_n, inertia_tensor_inv_b * r_cross_n) };
				float lambda{ -c / (p1.s.inverse_mass + rb_weight + compliance_term) };
				glm::vec3 p{ lambda * n };

				// Record particle's change in position.
				particle_delta_x += p * p1.s.inverse_mass;

				// Record rigid body's change in position and rotation.
				// For now just overwrite previous rigid body collisions this particle had. So particle will currently only influence one rigid body per time step.
				if (!rb->immovable)
				{
					rb_collision.rb_index = rb_idx;
					rb_collision.rb_delta_position = -p * rb_inv_mass;
					if (rb->voxel_chunk.IsPointMass()) {
						rb_collision.rb_delta_rotation = {};
					}
					else
					{
						glm::vec3 tmp2{ inertia_tensor_inv_b * glm::cross(r, p) };
						rb_collision.rb_delta_rotation = -0.5f * glm::quat{ 0.0f, tmp2.x, tmp2.y, tmp2.z } *rb->node->rotation;
					}
				}
			}
			++rb_idx;
		}

		return particle_delta_x;
	}

	std::vector<std::pair<float*, std::string>> CollisionConstraint::GetParameters()
	{
		return {
			{&compliance_, "Compliance"},
		};
	}

	void CollisionConstraint::OnParametersMutated()
	{
		// No-op.
	}
}
