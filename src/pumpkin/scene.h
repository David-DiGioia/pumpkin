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

		// Each transform is in local space of parent.
		glm::vec3 position{};
		glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
		glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

		Node* GetParent() const;

		const std::unordered_set<Node*>& GetChildren() const;

		void SetParent(Node* parent);

		void AddChild(Node* child);

		// Set the position in world space. Same as setting position directly if there is no parent.
		void SetWorldPosition(const glm::vec3& world_position);

		glm::vec3 GetWorldPosition() const;

		// Set the position in world space. Same as setting rotation directly if there is no parent.
		void SetWorldRotation(const glm::quat& world_rotation);

		glm::quat GetWorldRotation() const;

		glm::mat4 GetLocalTransform() const;

		// This recurses up the chain of parents, so if you are doing an operation starting at the root node
		// and recursing down, prefer to just pass the local transforms with you and accumulate to get global transforms.
		glm::mat4 GetWorldTransform() const;

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

		glm::mat4 GetProjectionMatrix(const renderer::Extent& viewport_extent) const;

		glm::mat4 GetProjectionViewMatrix(const renderer::Extent& viewport_extent) const;
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
