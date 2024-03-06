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

	class ParticleContext
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer, const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetParticles();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		uint32_t GenerateVoxelsOnNode(Node* node);

		uint32_t GenerateTestParticleOnNode(Node* node);

		void TransferStaticParticlesToMPM();

		MPMContext* GetMPMContext();

		renderer::VoxelChunk& GetVoxelChunk();

		void UpdatePhysicsRenderMaterials(std::vector<int>&& all_physics_render_materials);

#ifdef EDITOR_ENABLED
		void SetMPMDebugParticleGenEnabled(bool enabled);

		void SetMPMDebugNodeGenEnabled(bool enabled);
#endif

	private:
		std::vector<MaterialPoint> VoxelsToMaterialPoints(const renderer::VoxelChunk& voxel_chunk) const;

		void GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, std::vector<MaterialPoint>& particles) const;

		void GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target);

		void GenerateDynamicDebugMPMParticleInstances() const;

		void GenerateDynamicDebugMPMNodeInstances() const;

		renderer::VoxelChunk voxel_chunk_{ CHUNK_ROW_VOXEL_COUNT, CHUNK_ROW_VOXEL_COUNT, CHUNK_ROW_VOXEL_COUNT };
		bool has_played_{}; // True if the particle simulation has been played yet.
		bool update_physics_{};
#ifdef EDITOR_ENABLED
		bool generate_mpm_particle_instances_{};
		bool generate_mpm_node_instances_{};
#endif
		uint32_t generated_voxel_count_{}; // The current number of non-empty voxels generated.
		MPMContext mpm_context_{};
		renderer::VulkanRenderer* renderer_{};
		Node* particle_node_{};
		const std::vector<PhysicsMaterial*>* physics_materials_;
	};
}
