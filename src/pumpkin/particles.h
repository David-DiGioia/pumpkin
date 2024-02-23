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

		PhysicsMaterial* NewPhysicsMaterial();

		void DeletePhysicsMaterial(uint8_t physics_mat_index);

		// Set the physics material's index into render materials. Determines how each physics material is rendered.
		void SetPhysicsMaterialRender(uint8_t physics_mat_index, uint32_t render_mat_index);

		// Get the physics material's index into render materials.
		uint32_t GetPhysicsMaterialRender(uint8_t physics_mat_index);

		template<typename T>
		void SetPhysicsMaterialModel(uint8_t physics_mat_index)
		{
			mpm_context_.SetPhysicsMaterialModel<T>(physics_mat_index);
		}

		ConstitutiveModel* GetPhysicsMaterialModel(uint8_t physics_mat_index);

		std::vector<std::pair<float*, std::string>> GetPhysicsParameters(uint8_t physics_mat_index);

		void PhysicsParametersMutated(uint8_t physics_mat_index);

#ifdef EDITOR_ENABLED
		void SetMPMDebugParticleGenEnabled(bool enabled);

		void SetMPMDebugNodeGenEnabled(bool enabled);
#endif

	private:
		std::vector<MaterialPoint> StaticParticlesToMaterialPoints(const std::vector<renderer::StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags) const;

		void GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, std::vector<MaterialPoint>& particles) const;

		void GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target);

		void GenerateDynamicDebugMPMParticleInstances() const;

		void GenerateDynamicDebugMPMNodeInstances() const;

		void UpdatePhysicsRenderMaterials();

		std::vector<renderer::StaticParticle> static_particles_{};
		std::vector<uint8_t> side_flags_{};
		bool has_played_{}; // True if the particle simulation has been played yet.
		bool update_physics_{};
#ifdef EDITOR_ENABLED
		bool generate_mpm_particle_instances_{};
		bool generate_mpm_node_instances_{};
#endif
		MPMContext mpm_context_{};
		renderer::VulkanRenderer* renderer_{};
		Node* particle_node_{};
	};
}
