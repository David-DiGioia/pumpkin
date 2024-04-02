#pragma once

#include <vector>
#include <unordered_set>
#include <array>
#include <string>
#include <filesystem>
#include <stack>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "vulkan_renderer.h"
#include "physics.h"

namespace pmk
{
	struct Node
	{
		const uint32_t node_id;
		renderer::RenderObjectHandle render_object{ renderer::NULL_HANDLE };
		RigidBody* rigid_body{};

		// Each transform is in local space of parent.
		glm::vec3 position{};
		glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
		glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

		Node* GetParent() const;

		const std::unordered_set<Node*>& GetChildren() const;

		std::unordered_set<uint32_t> GetChildrenIDs() const;

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

		void SetWorldTransform(const glm::mat4& transform);

		// Not every transform matrix can be represented by position/rotation/scale, such as skew.
		void SetLocalTransform(const glm::mat4& transform);

		bool HasAncestor(Node* node);

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
		//
		// out_names writes the names of the imported nodes in order, or is ignored if null.
		void ImportGLTF(const std::filesystem::path& path, std::vector<std::string>* out_node_names, std::vector<std::string>* out_material_names);

		// Generate particles and attach render object to specified node.
		// Returns number of particles generated.
		uint32_t GenerateVoxelsOnNode(Node* node);

		// Returns number of particles generated.
		uint32_t GenerateTestParticleOnNode(Node* node);

		// Returns list of indices/IDs of nodes created (eg from rigid bodies).
		std::vector<uint32_t> PlayPhysicsSimulation();

		void PausePhysicsSimulation();

		void ResetPhysicsSimulation();

		bool GetPhysicsSimulationEnabled() const;

		bool GetParticleSimulationEmpty() const;

		void SetParticleOverlayEnabled(bool rasterize_particles, bool render_nodes);

		// Update all render objects transforms to reflect their containing node.
		void UploadRenderObjects();

		void UploadCamera();

		Camera& GetCamera();

		Node* CreateNode();

		// This should only be used when loading an existing node hierarchy from disk. Afterwards SetNextNodeID() MUST be called.
		Node* CreateNodeFromID(uint32_t id);

		void DestroyNode(Node* node);

		std::vector<Node*>& GetNodes();

		Node* GetRootNode() const;

		Node* GetNodeByRenderObject(renderer::RenderObjectHandle handle);

		void AddRenderObjectToNode(Node* node, renderer::RenderObjectHandle handle);

		void ParticlePhysicsUpdate(float delta_time);

		PhysicsMaterial* NewPhysicsMaterial();

		void DeletePhysicsMaterial(uint8_t physics_mat_index);

		// Set the physics material's index into render materials. Determines how each physics material is rendered.
		void SetPhysicsMaterialRender(uint8_t physics_mat_index, uint32_t render_mat_index);

		// Get the physics material's index into render materials.
		uint32_t GetPhysicsMaterialRender(uint8_t physics_mat_index);

		template<typename T>
		void SetPhysicsMaterialModel(uint8_t physics_mat_index)
		{
			physics_context_.SetPhysicsMaterialModel<T>(physics_mat_index);
		}

		ConstitutiveModel* GetPhysicsMaterialModel(uint8_t physics_mat_index);

		PhysicsMaterial* GetPhysicsMaterial(uint8_t physics_mat_index);

		std::vector<std::pair<float*, std::string>> GetPhysicsParameters(uint8_t physics_mat_index);

		void PhysicsParametersMutated(uint8_t physics_mat_index);

		// Write physics data to json.
		void DumpPhysicsMaterials(nlohmann::json& j);

		void LoadPhysicsMaterials(nlohmann::json& j);

	private:
		// Recursive implementation to upload node render object data.
		void UploadRenderObjectsRec(Node* root, const glm::mat4& parent_transform);

		Node* CreateNode(uint32_t id);

		Camera camera_{};
		renderer::VulkanRenderer* renderer_{};
		Node* root_node_{};
		std::vector<Node*> nodes_{};                                                       // All nodes in the scene. We heap allocate the nodes to avoid dangling pointers when nodes_ resizes.
		std::unordered_map<renderer::RenderObjectHandle, Node*> render_object_node_map_{}; // Map render object handles to nodes. This won't contain nodes without render objects.
		PhysicsContext physics_context_{};
		std::stack<uint32_t> vacant_node_indices_{}; // Unused node indices from deleted nodes to recycle.
	};
}
