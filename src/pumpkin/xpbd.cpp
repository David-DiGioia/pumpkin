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

	static uint32_t HashCoords(const glm::ivec3& coord)
	{
		int32_t h{ (coord.x * 92837111) ^ (coord.y * 689287499) ^ (coord.z * 283923481) };
		return (uint32_t)(std::abs(h) % HASH_TABLE_SIZE);
	}

	static uint32_t HashPosition(const glm::vec3& pos)
	{
		int32_t xi{ (int32_t)std::floorf(pos.x / GRID_SPACING) };
		int32_t yi{ (int32_t)std::floorf(pos.y / GRID_SPACING) };
		int32_t zi{ (int32_t)std::floorf(pos.z / GRID_SPACING) };
		return HashCoords({ xi, yi, zi });
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

	bool XPBDParticleIndex::operator<(const XPBDParticleIndex& other)
	{
		return key < other.key;
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
		particle_indices_.clear();
		particle_indices_.resize(particles_.size());
		hash_table_.clear();
		hash_table_.resize(HASH_TABLE_SIZE, NULL_INDEX);
		jacobi_positions_.resize(particles_.size());

		float particle_width = chunk_width / CHUNK_ROW_VOXEL_COUNT;
		particle_radius_ = 0.5f * particle_width;
		particle_initial_volume_ = particle_width * particle_width * particle_width;

		// Initialize particle mass and index buffer.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			particles_[i].inverse_mass = 1.0f / (GetPhysicsMaterial(particles_[i])->density * particle_initial_volume_);

			particle_indices_[i] = XPBDParticleIndex{
				.key = HashPosition(particles_[i].position),
				.index = i,
			};
		}

		UpdateIndexBuffers();
	}

	void XPBDParticleContext::SimulateStep(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		ApplyForces(delta_time);
		SolveConstraints(delta_time, rb_context);
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

	void XPBDParticleContext::ApplyForces(float delta_time)
	{
		for (XPBDParticle& p : particles_)
		{
			// Apply forces.
			p.velocity += delta_time * glm::vec3{ 0.0f, -9.8f, 0.0f }; // Gravity.

			// Predict position.
			p.predicted_position = p.position + delta_time * p.velocity;
		}
	}

	void XPBDParticleContext::SolveConstraints(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		// Preprocess each constraint.
		for (XPBDConstraint* constraint : *jacobi_constraints_) {
			constraint->Preprocess(this, rb_context, delta_time);
		}

		// Jacobi iterations.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			PhysicsMaterial* mat{ GetPhysicsMaterial(particles_[i]) };
			jacobi_positions_[i] = particles_[i].predicted_position;

			for (uint32_t j{ 0 }; j < (uint32_t)jacobi_constraints_->size(); ++j)
			{
				if (mat->jacobi_constraints_mask & (1 << j)) {
					jacobi_positions_[i] += (*jacobi_constraints_)[j]->Solve(this, rb_context, i, delta_time);
				}
			}
		}

		// Update position from Jacobi iterations.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i) {
			particles_[i].predicted_position = jacobi_positions_[i];
		}
	}

	void XPBDParticleContext::UpdateVelocityAndInternalForces(float delta_time)
	{
		for (XPBDParticle& p : particles_)
		{
			// Update velocity.
			p.velocity = (p.predicted_position - p.position) / delta_time;

			// In the future, internal forces like drag and vorticity will be applied here.

			// Update position.
			p.position = p.predicted_position;
		}
	}

	float XPBDParticleContext::ComputeDensity(const glm::vec3& pos, const ConstIndexProximityContainer& proximity_particles) const
	{
		float density{ 0.0f };

		uint32_t i{ 0 };
		//for (uint32_t j{ 0 }; j < (uint32_t)particles_.size(); ++j)
		for (uint32_t j : proximity_particles)
		{
			++i;
			float delta_pos{ glm::length(pos - particles_[j].position) };
			if (delta_pos < SPH_KERNEL_RADIUS) {
				density += (1.0f / particles_[j].inverse_mass) * SPHKernel(delta_pos);
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
		glm::ivec3 coord{ glm::ivec3{
			(int32_t)std::floorf(position.x / GRID_SPACING),
			(int32_t)std::floorf(position.y / GRID_SPACING),
			(int32_t)std::floorf(position.z / GRID_SPACING),
		} };

		uint32_t result_idx{ 0 };
		for (int32_t i{ coord.x - 1 }; i <= coord.x + 1; ++i)
		{
			for (int32_t j{ coord.y - 1 }; j <= coord.y + 1; ++j)
			{
				for (int32_t k{ coord.z - 1 }; k <= coord.z + 1; ++k)
				{
					glm::ivec3 neighbor_coord{ i, j, k };
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


	void XPBDParticleContext::UpdateIndexBuffers()
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
					hash_table_[HashPosition(p.position)] = i;
					current_key = particle_indices_[i].key;
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
			constexpr float compliance{ 0.0f };
			constexpr float alpha{ compliance };
			float alpha_tilde{ alpha / (delta_time * delta_time) };

			auto proximity_particles{ p_context->GetParticleIndicesByProximity(particles[i].position) };
			// TODO: Add rigid body density.
			float density{ p_context->ComputeDensity(particles[i].position, proximity_particles) };
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

				glm::vec3 q{ particles[i].position - particles[k].position };
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


	glm::vec3 FluidDensityConstraint::Solve(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const
	{
		const XPBDParticle& particle{ p_context->GetParticles()[particle_idx] };

		glm::vec3 delta_x{};
		for (uint32_t j : p_context->GetParticleIndicesByProximity(particle.position))
		{
			if (particle_idx == j) {
				continue;
			}

			glm::vec3 q{ particle.position - p_context->GetParticles()[j].position };
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
}
