#pragma once

#include <vector>
#include <type_traits>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "constraint.h"
#include "logger.h"
#include "common_constants.h"

#define GAUSS_SEIDEL_WITHIN_CHUNK 1

namespace pmk
{
	// Bare essential members of particle needed for Solve() function. Stripped down to stay hot in the cache.
	struct alignas(32) XPBDParticleStripped
	{
		glm::vec3 position;           // Meters.
		glm::vec3 predicted_position; // Meters.
		float inverse_mass;           // Reciprocal kilograms.
	};

	// Auxillary particle data not needed in Solve() function.
	struct XPBDParticle
	{
		uint64_t key;       // Sort the particles optimally for lookup and for the cache.
		glm::vec3 velocity; // Meters per second.
		uint8_t physics_material_index;

		// XPBDParticle members to copy to XPBDParticle after sort.
		struct
		{
			glm::vec3 position;           // Meters.
			glm::vec3 predicted_position; // Meters.
			float inverse_mass;           // Reciprocal kilograms.
		} s;

#ifdef EDITOR_ENABLED
		glm::vec3 debug_color; // Color to show arbitrary debug information when debugging.
#endif
	};

	struct RigidBodyParticleCollisionInfo
	{
		uint32_t rb_index;
		glm::vec3 rb_delta_position;
		glm::quat rb_delta_rotation;
	};

	class XPBDRigidBodyContext;
	struct PhysicsMaterial;

	class XPBDParticleContext
	{
	public:
		void Initialize(
			float chunk_width,
			const std::vector<XPBDConstraint*>* jacobi_constraints,
			const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		void AddParticles(std::vector<XPBDParticle>&& particles);

		void SimulateStep(float delta_time, const XPBDRigidBodyContext* rb_context);

		const std::vector<XPBDParticle>& GetParticles() const;

		std::vector<XPBDParticle>& GetParticles();

		const XPBDParticleStripped* GetParticlesStripped() const;

		uint32_t GetParticleCount() const;

#if GAUSS_SEIDEL_WITHIN_CHUNK
		const XPBDParticleStripped* GetParticlesScratch() const;
#endif

		const std::vector<uint32_t>& GetParticleKeys() const;

		RigidBodyParticleCollisionInfo& GetRigidBodyCollision(uint32_t particle_idx);

		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> GetParticleRangesWithinKernelSIMD(const glm::vec3& position) const;

		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> GetParticleRangesWithinKernel(const glm::vec3& position, uint32_t* out_block_count) const;

		const std::vector<std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL>>& GetCachedParticleRanges() const;

	private:
		void ApplyForces(float delta_time);

		void PrecomputeParticleRanges();

		void SolveConstraints(float delta_time, const XPBDRigidBodyContext* rb_context);

		void UpdateVelocityAndInternalForces(float delta_time);

		void UpdateIndexBuffers();

		// Copy particles positions to stripped particles.
		void CopyPositions();

		const PhysicsMaterial* GetPhysicsMaterial(const XPBDParticle& p) const;

		PhysicsMaterial* GetPhysicsMaterial(const XPBDParticle& p);

		XPBDParticleStripped* particles_stripped_{};                                    // Stripped down particle needed in Solve(). Not in vector so it can be allocated with custom alignment.
#if GAUSS_SEIDEL_WITHIN_CHUNK										                    
		XPBDParticleStripped* particles_scratch_{};                                     // Buffer for particles to use during calculations in a substep. Particularly, for gauss-seidell style solving within a chunk.
#endif																                    
		std::vector<XPBDParticle> particles_{};                                         // All particle members not needed in Solve().
		std::vector<uint32_t> particle_keys_{};                                         // Keys of particles put into separate buffer to stay hot in cache during Solve().
		std::vector<uint32_t> hash_table_{};                                            // Indices into particle_indices_, showing start of contiguous region containing particles with this hash value.
		std::vector<std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL>> particle_ranges_{}; // The ith index contains an index into particles_ of the start of a range. We store a buffer to precompute the values.
		std::vector<RigidBodyParticleCollisionInfo> rb_collisions_{};                   // The ith index cooresponds to particles_[i] collision with a rigid body.

		const std::vector<XPBDConstraint*>* jacobi_constraints_{};
		const std::vector<PhysicsMaterial*>* physics_materials_{};

		float particle_radius_{};
		float particle_initial_volume_{};
	};

	class FluidCollisionConstraint : public XPBDConstraint
	{
	public:
		FluidCollisionConstraint();

		virtual glm::vec3 Solve(
			XPBDParticleContext* p_context,
			const XPBDRigidBodyContext* rb_context,
			uint32_t particle_idx,
			float delta_time,
			uint32_t chunk_begin,
			uint32_t chunk_end) const override;

		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		virtual void OnParametersMutated() override;

	protected:
		friend class PhysicsContext;

		float collision_compliance_{ 0.0f };
		float attractive_compliance_{ 0.01f };
		float repulsive_compliance_{ 0.01f };
		float attractive_width_multiplier_{ 1.8f };
		float repulsive_width_multiplier_{ 1.4f };

		float attractive_width_{ PARTICLE_WIDTH * attractive_width_multiplier_ };
		float repulsive_width_{ PARTICLE_WIDTH * repulsive_width_multiplier_ };
		float attractive_width_squared_{ attractive_width_ * attractive_width_ };
		float repulsive_width_squared_{ repulsive_width_ * repulsive_width_ };
	};

	class GranularConstraint : public XPBDConstraint
	{
	public:
		GranularConstraint();

		virtual glm::vec3 Solve(
			XPBDParticleContext* p_context,
			const XPBDRigidBodyContext* rb_context,
			uint32_t particle_idx,
			float delta_time,
			uint32_t chunk_begin,
			uint32_t chunk_end) const override;

		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		virtual void OnParametersMutated() override;

	protected:
		friend class PhysicsContext;

		float compliance_{ 0.0f };
		float static_friction_{0.6f};
		float dynamic_friction_{0.5f};
	};

}
