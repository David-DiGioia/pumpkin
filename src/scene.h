#pragma once

#include <vector>
#include <array>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "mesh_types.h"
#include "vulkan_renderer.h"

constexpr uint32_t NULL_INDEX{ 0xFFFFFFFF };

struct Node
{
	// TODO: Later handle multiple primitives per mesh from GLTF file.
	//       This occurs when a single mesh has multiple materials.
	//       For raytracing we probably want to implement with geometry indexing.
	uint32_t render_object_idx{ NULL_INDEX };
	glm::vec3 translation{};
	glm::vec3 scale{};
	glm::quat rotation{};
	std::vector<Node*> children{};
};

// Scene resources that must have unique copies for each frame in flight.
// This will only be renderer releated objects, since the last frame's
// resources may still be in use when we start the next frame.
struct SceneFrameResources
{
	std::vector<renderer::RenderObject> render_objects{}; // Nodes may optionally reference a render object.
};

class Scene
{
public:
	void Initialize(renderer::VulkanRenderer* renderer);

	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	void ImportGLTF(const std::string& path);

	// Create render object, adding it to both frame resources lists.
	//
	// Returns index of render object.
	uint32_t CreateRenderObject(renderer::Mesh* mesh);

	std::vector<renderer::RenderObject>* GetRenderObjects();

	// Update all render objects transforms to reflect their containing node.
	void UpdateRenderObjects();

	void DrawScene(renderer::VulkanRenderer& renderer);

	renderer::RenderObject* GetRenderObject(uint32_t idx);

private:
	renderer::VulkanRenderer* renderer_{};
	std::vector<Node*> root_nodes_{};
	std::vector<Node> nodes_{}; // All nodes in the scene.
	std::vector<renderer::Mesh> meshes_{}; // All meshes used by nodes.

	std::array<SceneFrameResources, renderer::FRAMES_IN_FLIGHT> frame_resources_{};
	uint32_t current_frame_{};
};
