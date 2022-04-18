#include "scene.h"

// Disable warnings from tiny_gltf.
#include <codeanalysis\warnings.h>
#pragma warning( push )
#pragma warning ( disable : ALL_CODE_ANALYSIS_WARNINGS )
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#pragma warning( pop )

#include "logger.h"
#include "mesh.h"
#include "glm/gtx/transform.hpp"

void Scene::ImportGLTF(renderer::VulkanRenderer* renderer, const std::string& path)
{
	logger::Print("Loading glTF file: %s\n", path.c_str());

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.c_str());
	//bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

	if (!warn.empty()) {
		logger::Error("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		logger::Error("Err: %s\n", err.c_str());
	}

	if (!ret) {
		logger::Error("Failed to parse glTF\n");
	}

	meshes_.reserve(model.meshes.size());
	renderer->LoadMeshesGLTF(model, &meshes_);

	int i{ (int)nodes_.size() };
	nodes_.resize(nodes_.size() + model.nodes.size());
	for (tinygltf::Node gltf_node : model.nodes)
	{
		Node node{};

		if (gltf_node.mesh >= 0)
		{
			renderer::Mesh* mesh_ptr = &meshes_[gltf_node.mesh];
			node.render_object = CreateRenderObject(mesh_ptr);

			if (!gltf_node.translation.empty()) {
				node.translation = glm::vec3{ (float)gltf_node.translation[0], (float)gltf_node.translation[1], (float)gltf_node.translation[2] };
			}

			if (!gltf_node.scale.empty()) {
				node.scale = glm::vec3{ (float)gltf_node.scale[0], (float)gltf_node.scale[1], (float)gltf_node.scale[2] };
			}

			if (!gltf_node.rotation.empty()) {
				node.rotation = glm::quat{ (float)gltf_node.rotation[0], (float)gltf_node.rotation[1], (float)gltf_node.rotation[2], (float)gltf_node.rotation[3] };
			}

			for (int child_idx : gltf_node.children) {
				node.children.push_back(&nodes_[child_idx]);
			}
		}
		nodes_[i++] = node;
	}
}

renderer::RenderObject* Scene::CreateRenderObject(renderer::Mesh* mesh)
{
	render_objects_.emplace_back();
	render_objects_.back().mesh = mesh;
	render_objects_.back().vertex_type = renderer::VertexType::POSITION_NORMAL_COORD;
	render_objects_.back().transform = glm::mat4(1.0f);
	return &render_objects_.back();
}

std::vector<renderer::RenderObject>* Scene::GetRenderObjects()
{
	return &render_objects_;
}

void Scene::UpdateRenderObjects()
{
	for (Node& node : nodes_)
	{
		if (node.render_object)
		{
			glm::mat4 scale_mat{ glm::scale(node.scale) };
			glm::mat4 rotation_mat{ glm::toMat4(node.rotation) };
			glm::mat4 translate_mat{ glm::translate(node.translation) };

			node.render_object->transform = translate_mat * rotation_mat * scale_mat;
		}
	}
}
