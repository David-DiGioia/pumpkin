#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "pipeline.h"
#include "xpbd.h"
#include "vulkan_renderer.h"

namespace pmk
{
	struct Node;
	struct RigidBody;

	class VoxelContext
	{
	public:
		void Initialize(
			renderer::VulkanRenderer* renderer,
			const std::vector<XPBDConstraint*>* jacobi_constraints,
			const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		void PhysicsUpdate(float delta_time, const std::vector<RigidBody*>& rigid_bodies);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetParticles();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		uint32_t GenerateVoxelsOnNode(Node* node);

		void TransferStaticParticlesToXPBD();

		XPBDParticleContext* GetXPBDContext();

		renderer::VoxelChunk& GetVoxelChunk();

		void UpdatePhysicsRenderMaterials(std::vector<int>&& all_physics_render_materials);

		void DestroyVoxelRenderObject();

#ifdef EDITOR_ENABLED
		void SetMPMDebugParticleGenEnabled(bool enabled);
#endif

	private:
		std::vector<XPBDParticle> VoxelsToMaterialPoints(const renderer::VoxelChunk& voxel_chunk) const;

		void GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, std::vector<XPBDParticle>& particles) const;

		void GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target);

		void GenerateDynamicDebugMPMParticleInstances() const;

		renderer::VoxelChunk voxel_chunk_{ CHUNK_ROW_VOXEL_COUNT, CHUNK_ROW_VOXEL_COUNT, CHUNK_ROW_VOXEL_COUNT };
		bool has_played_{}; // True if the particle simulation has been played yet.
		bool update_physics_{};
#ifdef EDITOR_ENABLED
		bool generate_mpm_particle_instances_{};
#endif
		uint32_t generated_voxel_count_{}; // The current number of non-empty voxels generated.
		XPBDParticleContext xpbd_context_{};
		renderer::VulkanRenderer* renderer_{};
		Node* particle_node_{};
		const std::vector<PhysicsMaterial*>* physics_materials_;
		const std::vector<XPBDConstraint*>* jacobi_constraints_;
	};
}
