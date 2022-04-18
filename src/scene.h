#pragma once

#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "mesh_types.h"
#include "vulkan_renderer.h"

struct Node
{
	// TODO: Later handle multiple primitives per mesh from GLTF file.
	//       This occurs when a single mesh has multiple materials.
	//       For raytracing we probably want to implement with geometry indexing.
	renderer::RenderObject* render_object{};
	glm::vec3 translation{};
	glm::vec3 scale{};
	glm::quat rotation{};

	std::vector<Node*> children{};
};

class Scene
{
public:
	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	void ImportGLTF(renderer::VulkanRenderer* renderer, const std::string& path);

	// Create render object, adding it to list.
	renderer::RenderObject* CreateRenderObject(renderer::Mesh* mesh);

	std::vector<renderer::RenderObject>* GetRenderObjects();

	// Update all render objects transforms to reflect their containing node.
	void UpdateRenderObjects();

private:
	std::vector<Node*> root_nodes_{};
	std::vector<Node> nodes_{}; // All nodes in the scene.
	std::vector<renderer::RenderObject> render_objects_{}; // Nodes may optionally reference a render object.
	std::vector<renderer::Mesh> meshes_{}; // All meshes used by nodes.
};
