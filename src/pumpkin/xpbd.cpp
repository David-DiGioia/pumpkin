#include "xpbd.h"

#include <cmath>
#include "tracy/Tracy.hpp"
#include "glm/gtx/norm.hpp"

#include "common_constants.h"
#include "physics.h"

namespace pmk
{
	constexpr uint32_t HASH_TABLE_SIZE{ 64000 };
	constexpr float GRID_SPACING{ PARTICLE_WIDTH * 3.0f };
	constexpr float SPH_KERNEL_RADIUS{ GRID_SPACING };
	constexpr float SPH_KERNEL_RADIUS_SQUARED{ SPH_KERNEL_RADIUS * SPH_KERNEL_RADIUS };

	static uint32_t HashCoords(const glm::vec3& pos)
	{
		int32_t xi{ (int32_t)std::floorf(pos.x / GRID_SPACING) };
		int32_t yi{ (int32_t)std::floorf(pos.y / GRID_SPACING) };
		int32_t zi{ (int32_t)std::floorf(pos.z / GRID_SPACING) };
		int32_t h{ (xi * 92837111) ^ (yi * 689287499) ^ (zi * 283923481) };
		return (uint32_t)(std::abs(h) % HASH_TABLE_SIZE);
	}

	// TODO: Maybe replace kernel and gradient with lookup table.
	static float SPHKernel(float q)
	{
		constexpr float sigma_3{ 1.0f / (PI * SPH_KERNEL_RADIUS * SPH_KERNEL_RADIUS * SPH_KERNEL_RADIUS) };

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

	static glm::vec3 SPHKernelGradient(float q)
	{
		// TODO
		return {};
	}

	bool XPBDParticleIndex::operator<(const XPBDParticleIndex& other)
	{
		return key < other.key;
	}

	void XPBDContext::Initialize(
		std::vector<XPBDParticle>&& particles,
		float chunk_width,
		const std::vector<XPBDConstraint*>* jacobi_constraints,
		const std::vector<PhysicsMaterial*>* physics_materials)
	{
		particles_ = std::move(particles);
		jacobi_constraints_ = jacobi_constraints;
		physics_materials_ = physics_materials;
		particle_indices_.clear();
		particle_indices_.resize(particles_.size());
		hash_table_.clear();
		hash_table_.resize(HASH_TABLE_SIZE, NULL_INDEX);

		jacobi_positions_.resize(particles_.size());

		float particle_width = chunk_width / CHUNK_ROW_VOXEL_COUNT;
		particle_radius_ = 0.5f * particle_width;
		particle_initial_volume_ = particle_width * particle_width * particle_width;

		// Initialize particle index buffer.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			particle_indices_[i] = XPBDParticleIndex{
				.key = HashCoords(particles_[i].position),
				.index = i,
			};
		}

		UpdateIndexBuffers();
	}

	void XPBDContext::SimulateStep(float delta_time, const std::vector<RigidBody*>& rigid_bodies)
	{
		ApplyForces(delta_time);
		SolveConstraints(delta_time);
		UpdateVelocityAndInternalForces();

		UpdateIndexBuffers();
	}

	const std::vector<XPBDParticle>& XPBDContext::GetParticles() const
	{
		return particles_;
	}

	std::vector<XPBDParticle>& XPBDContext::GetParticles()
	{
		return particles_;
	}

	void XPBDContext::ApplyForces(float delta_time)
	{
		for (XPBDParticle& p : particles_)
		{
			// Apply forces.
			p.velocity += delta_time * glm::vec3{ 0.0f, -9.8f, 0.0f }; // Gravity.

			// Predict position.
			p.predicted_position = p.position + delta_time * p.velocity;
		}
	}

	void XPBDContext::SolveConstraints(float delta_time)
	{
		// Preprocess each constraint.
		for (XPBDConstraint* constraint : *jacobi_constraints_) {
			constraint->Preprocess(this, delta_time);
		}

		// Jacobi iterations.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			PhysicsMaterial* mat{ (*physics_materials_)[particles_[i].physics_material_index] };

			for (uint32_t j{ 0 }; j < (uint32_t)jacobi_constraints_->size(); ++j)
			{
				if (mat->jacobi_constraints_mask & (1 << j)) {
					jacobi_positions_[i] += (*jacobi_constraints_)[j]->Solve(this, i, delta_time);
				}
			}
		}

		// Update position from Jacobi iterations.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i) {
			particles_[i].position = jacobi_positions_[i];
		}
	}

	void XPBDContext::UpdateVelocityAndInternalForces()
	{
		for (XPBDParticle& p : particles_)
		{
			// Update velocity.
			p.velocity = 0.5f * (p.predicted_position - p.position);

			// In the future, internal forces like drag and vorticity will be applied here.

			// Update position.
			p.position = p.predicted_position;
		}
	}

	float XPBDContext::ComputeDensity(const glm::vec3& pos, const ConstProximityContainer& proximity_particles) const
	{
		float density{ 0.0f };

		for (const XPBDParticle& p : proximity_particles)
		{
			float delta_pos{ glm::length(pos - p.position) };
			if (delta_pos < SPH_KERNEL_RADIUS) {
				density += (1.0f / p.inverse_mass) * SPHKernel(delta_pos);
			}
		}

		return density;
	}

	XPBDContext::ProximityContainer XPBDContext::GetParticlesByProximity(const glm::vec3& position)
	{
		return ProximityContainer(this, position);
	}

	XPBDContext::ConstProximityContainer XPBDContext::GetParticlesByProximity(const glm::vec3& position) const
	{
		return ConstProximityContainer(this, position);
	}

	XPBDContext::IndexProximityContainer XPBDContext::GetParticleIndicesByProximity(const glm::vec3& position)
	{
		return IndexProximityContainer(this, position);
	}

	XPBDContext::ConstIndexProximityContainer XPBDContext::GetParticleIndicesByProximity(const glm::vec3& position) const
	{
		return ConstIndexProximityContainer(this, position);
	}

	std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> XPBDContext::GetParticleRangesWithinKernel(const glm::vec3& position, uint32_t* out_block_count) const
	{
		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> result{};
		glm::uvec3 coord{ glm::uvec3{position} };

		uint32_t result_idx{ 0 };
		for (uint32_t i{ coord.x - 1 }; i <= coord.x + 1; ++i)
		{
			for (uint32_t j{ coord.y - 1 }; j <= coord.y + 1; ++j)
			{
				for (uint32_t k{ coord.z - 1 }; k <= coord.z + 1; ++k)
				{
					glm::uvec3 neighbor_coord{ i, j, k };
					uint32_t particle_idx_idx{ hash_table_[HashCoords(neighbor_coord)] };
					if (particle_idx_idx != NULL_INDEX) {
						result[result_idx++] = particle_idx_idx;
					}
				}
			}
		}

		*out_block_count = result_idx;
		return result;
	}


	void XPBDContext::UpdateIndexBuffers()
	{
		ZoneScoped;
		{
			ZoneScopedN("Group");
			// Grouping is significantly faster than sorting here.
			GroupByKey<XPBDParticleIndex, HASH_TABLE_SIZE>(particle_indices_);
		}

		hash_table_.clear();
		hash_table_.resize(HASH_TABLE_SIZE, NULL_INDEX);

		{
			ZoneScopedN("Update keys");
			uint32_t current_key{ NULL_INDEX };
			for (uint32_t i{ 0 }; i < (uint32_t)particle_indices_.size(); ++i)
			{
				if ((particle_indices_[i].key != current_key) && (particle_indices_[i].key != NULL_INDEX))
				{
					XPBDParticle& p{ particles_[particle_indices_[i].index] };
					hash_table_[HashCoords(p.position)] = i;
					current_key = particle_indices_[i].key;
				}
			}
		}
	}

	void FluidDensityConstraint::Preprocess(const XPBDContext* context, float delta_time)
	{
		lambda_cache_.resize(context->GetParticles().size());
		const std::vector<XPBDParticle>& particles{ context->GetParticles() };

		// Compute lambda corresponding to each particle. It will be used in Solve().
		for (uint32_t i{ 0 }; i < (uint32_t)particles.size(); ++i)
		{
			constexpr float compliance{ 0.0f };
			constexpr float alpha{ compliance };
			float alpha_tilde{ alpha / (delta_time * delta_time) };

			auto proximity_particles{ context->GetParticlesByProximity(particles[i].position) };
			float density{ context->ComputeDensity(particles[i].position, proximity_particles) };
			float c{ (density / rest_density_) - 1.0f };

			float denominator{ 0.0f };
			for (const XPBDParticle& p : proximity_particles)
			{
				float q{ glm::distance(particles[i].position, p.position) };
				denominator += p.inverse_mass * glm::length2(SPHKernelGradient(q));
			}

			lambda_cache_[i] = -c / (denominator + alpha_tilde);
		}
	}

	glm::vec3 FluidDensityConstraint::Solve(const XPBDContext* context, uint32_t particle_idx, float delta_time) const
	{
		const XPBDParticle& particle{ context->GetParticles()[particle_idx] };

		glm::vec3 delta_x{};
		for (uint32_t j : context->GetParticleIndicesByProximity(particle.position))
		{
			float q{ glm::distance(particle.position, context->GetParticles()[j].position) };
			delta_x += (lambda_cache_[particle_idx] + lambda_cache_[j]) * SPHKernelGradient(q); // From Survey of PBD 2017 fluid section.
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
}
