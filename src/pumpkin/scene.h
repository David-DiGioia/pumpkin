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

		glm::vec3 position{};
		glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
		glm::quat rotation{};

		std::vector<Node*> children{};
	};

	struct Camera
	{
		glm::vec3 position{};
		glm::quat rotation{};
		float fov{ 45.0f };
		float near_plane{ 0.1f };

		glm::mat4 GetViewMatrix() const;
	};

	class Scene
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer);

		// Import the whole GLTF hierarchy, adding all nodes to scene.
		// Note that Blender doesn't export cameras or lights.
		void ImportGLTF(const std::string& path);

		// Update all render objects transforms to reflect their containing node.
		void UploadRenderObjects();

		void UploadCamera();

		Camera& GetCamera();

	private:
		Camera camera_{};
		renderer::VulkanRenderer* renderer_{};
		std::vector<Node*> root_nodes_{};
		std::vector<Node> nodes_{}; // All nodes in the scene.
	};
}
