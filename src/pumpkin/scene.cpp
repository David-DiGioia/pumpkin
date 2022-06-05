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

namespace pmk
{
	void Scene::Initialize(renderer::VulkanRenderer* renderer)
	{
		renderer_ = renderer;
	}

	void Scene::ImportGLTF(const std::string& path)
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

		renderer_->LoadMeshesGLTF(model);

		int i{ (int)nodes_.size() };
		nodes_.resize(nodes_.size() + model.nodes.size());
		for (tinygltf::Node gltf_node : model.nodes)
		{
			Node node{};

			if (gltf_node.mesh >= 0)
			{
				node.render_object = renderer_->CreateRenderObject(gltf_node.mesh);

				if (!gltf_node.translation.empty()) {
					node.position = glm::vec3{ (float)gltf_node.translation[0], (float)gltf_node.translation[1], (float)gltf_node.translation[2] };
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

	void Scene::UploadRenderObjects()
	{
		for (Node& node : nodes_)
		{
			// Not every node has a render object.
			if (node.render_object == renderer::NULL_HANDLE) {
				continue;
			}

			glm::mat4 scale_mat{ glm::scale(node.scale) };
			glm::mat4 rotation_mat{ glm::toMat4(node.rotation) };
			glm::mat4 translate_mat{ glm::translate(node.position) };

			renderer_->SetRenderObjectTransform(node.render_object, translate_mat * rotation_mat * scale_mat);
		}
	}

	void Scene::UploadCamera()
	{
		renderer_->SetCameraMatrix(camera_.GetViewMatrix(), camera_.fov, camera_.near_plane);
	}

	Camera& Scene::GetCamera()
	{
		return camera_;
	}

	glm::mat4 Camera::GetViewMatrix() const
	{
		return glm::inverse(glm::translate(position) * glm::toMat4(rotation));
	}
}
