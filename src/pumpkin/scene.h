#pragma once

#include <vector>
#include <unordered_set>
#include <array>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "vulkan_renderer.h"

namespace pmk
{
	struct Node
	{
		const uint32_t node_id;
		renderer::RenderObjectHandle render_object{ renderer::NULL_HANDLE };

		glm::vec3 position{};
		glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
		glm::quat rotation{};

		Node* GetParent() const;

		const std::unordered_set<Node*>& GetChildren() const;

		void SetParent(Node* parent);

		void AddChild(Node* child);

	private:
		// Make constructor private to insure node_id is assigned only from Scene.
		Node(uint32_t id);
		friend class Scene;

		Node* parent_{};
		std::unordered_set<Node*> children_{};
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

		void CleanUp();

		// Import the whole GLTF hierarchy, adding all nodes to scene.
		// Note that Blender doesn't export cameras or lights.
		void ImportGLTF(const std::string& path);

		// Update all render objects transforms to reflect their containing node.
		void UploadRenderObjects();

		void UploadCamera();

		Camera& GetCamera();

		Node* CreateNode();

		std::vector<Node*>& GetNodes();

		Node* GetRootNode() const;

	private:
		// Recursive implementation to upload node render object data.
		void UploadRenderObjectsRec(Node* root, const glm::mat4& parent_transform);

		Camera camera_{};
		renderer::VulkanRenderer* renderer_{};
		Node* root_node_{};
		std::vector<Node*> nodes_{}; // All nodes in the scene. We heap allocate the nodes to avoid dangling pointers when nodes_ resizes.
		uint32_t next_node_id_{};    // The next node created will have this id.
	};
}
