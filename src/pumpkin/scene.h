#pragma once

#include <vector>
#include <array>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "vulkan_renderer.h"

namespace pmk
{
	struct Node
	{
		renderer::RenderObjectHandle render_object{ renderer::NULL_HANDLE };

		glm::vec3 translation{};
		glm::vec3 scale{};
		glm::quat rotation{};

		std::vector<Node*> children{};
	};

	class Scene
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer);

		// Import the whole GLTF hierarchy, adding all nodes to scene.
		// Note that Blender doesn't export cameras or lights.
		void ImportGLTF(const std::string& path);

		// Update all render objects transforms to reflect their containing node.
		void UpdateRenderObjects();

	private:
		renderer::VulkanRenderer* renderer_{};
		std::vector<Node*> root_nodes_{};
		std::vector<Node> nodes_{}; // All nodes in the scene.
	};
}
