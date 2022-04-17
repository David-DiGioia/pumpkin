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
	renderer::Mesh* mesh;
	renderer::VertexType vertex_type;
	glm::vec3 translation;
	glm::vec3 scale;
	glm::quat rotation;

	std::vector<Node*> children;
};

class Scene
{
public:
	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	void ImportGLTF(renderer::VulkanRenderer* renderer, const std::string& path);

private:
	std::vector<Node*> root_nodes_;
	std::vector<Node> nodes_; // All nodes in the scene.
	std::vector<renderer::Mesh> meshes_; // All meshes used by nodes.
};
