#pragma once

#include "vulkan_renderer.h"

namespace pmk
{
	class Scene;
	class Node;
	class RigidBody;
	class XPBDParticleContext;

	class Chunk
	{
	public:
		void Initialize(Node* node, renderer::VulkanRenderer* renderer, XPBDParticleContext* xpbd_context);

		void CleanUp();

		void GenerateVoxels();

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

	private:
		void CreateChunks();

		Scene* scene_{};
		renderer::VulkanRenderer* renderer_{};
		XPBDParticleContext* xpbd_context_{};
		std::vector<Chunk> chunks_{};
	};
}
