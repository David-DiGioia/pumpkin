#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "pipeline.h"
#include "mpm.h"
#include "vulkan_renderer.h"

namespace pmk
{
	struct Node;

	// TODO: Should eventually include parameters of constitutive model here.
	struct ParticleType
	{
		ConstitutiveModelIndex constitutive_model;
		uint32_t material_index;
	};

	// Convert a particle 1D buffer index into a 3D coordinate in the chunk.
	glm::uvec3 ParticleIndexToCoordinate(uint32_t index);

	// Convert 3D coordinate in the chunk into a 1D buffer index.
	uint32_t CoordinateToParticleIndex(const glm::uvec3& coord);

	class ParticleContext
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetParticles();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		uint32_t GenerateParticlesOnNode(Node* node);

		uint32_t GenerateTestParticleOnNode(Node* node);

		void TransferStaticParticlesToMPM();

	private:
		void GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, const std::vector<MaterialPoint>& particles) const;

		std::vector<renderer::StaticParticle> static_particles_{};
		bool has_played_{}; // True if the particle simulation has been played yet.
		bool update_physics_{};
		MPMContext mpm_context_{};
		renderer::VulkanRenderer* renderer_{};
		Node* particle_node_{};
	};
}
