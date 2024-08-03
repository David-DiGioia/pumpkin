#pragma once

#include "vulkan_renderer.h"
#include "rigid_body.h"

namespace pmk
{
	class Scene;
	class Node;
	class XPBDParticleContext;

	class Chunk
	{
	public:
		void Initialize(Node* node, renderer::VulkanRenderer* renderer, XPBDParticleContext* xpbd_context);

		void CleanUp();

		void GenerateVoxels();

		Node* GetNode();

	private:

		void GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target);

		void TransferVoxelsToXPBD();

		void DestroyRenderObject();

		renderer::VulkanRenderer* renderer_{};
		XPBDParticleContext* xpbd_context_{};
		Node* node_{};
		RigidBody rb_{};
		uint32_t generated_voxel_count_{};
	};

	class Terrain
	{
	public:
		void Initialize(Scene* scene, renderer::VulkanRenderer* renderer, XPBDParticleContext* xpbd_context);

		void CleanUp();

		void GenerateVoxels();

		void UpdatePhysicsRenderMaterials(std::vector<int>&& all_physics_render_materials);

	private:
		void CreateChunks();

		Scene* scene_{};
		renderer::VulkanRenderer* renderer_{};
		XPBDParticleContext* xpbd_context_{};
		std::vector<Chunk> chunks_{};
	};
}
