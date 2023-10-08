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
	Node::Node(uint32_t id)
		: node_id{ id }
	{}

	Node* Node::GetParent() const
	{
		return parent_;
	}

	const std::unordered_set<Node*>& Node::GetChildren() const
	{
		return children_;
	}

	std::unordered_set<uint32_t> Node::GetChildrenIDs() const
	{
		std::unordered_set<uint32_t> ids{};

		for (const pmk::Node* child : children_) {
			ids.insert(child->node_id);
		}

		return ids;
	}

	void Node::SetParent(Node* parent)
	{
		// If setting this parent would create a parent cycle, abort.
		if (parent && parent->HasAncestor(this)) {
			return;
		}

		glm::mat4 original_world_transform{ GetWorldTransform() };

		// No longer child of old parent.
		if (parent_) {
			parent_->children_.erase(this);
		}

		// Make child of new parent.
		parent_ = parent;
		if (parent) {
			parent->children_.insert(this);
		}

		SetWorldTransform(original_world_transform);
	}

	void Node::AddChild(Node* child)
	{
		if (child)
		{
			// No longer child of old parent.
			if (child->parent_) {
				child->parent_->children_.erase(child);
			}

			// Make child of this parent.
			children_.insert(child);
			child->parent_ = this;
		}
	}

	void Node::SetWorldPosition(const glm::vec3& world_position)
	{
		// Because (parent world space transform) * (local transform) = (world transform).
		position = glm::inverse(parent_->GetWorldTransform()) * glm::vec4{ world_position, 1.0f };
	}

	glm::vec3 Node::GetWorldPosition() const
	{
		return parent_->GetWorldTransform() * glm::vec4{ position, 1.0f };
	}

	void Node::SetWorldRotation(const glm::quat& world_rotation)
	{
		rotation = glm::inverse(parent_->GetWorldRotation()) * world_rotation;
	}

	glm::quat Node::GetWorldRotation() const
	{
		if (parent_) {
			return parent_->GetWorldRotation() * rotation;
		}
		return rotation;
	}

	glm::mat4 Node::GetLocalTransform() const
	{
		return glm::translate(position) * glm::toMat4(rotation) * glm::scale(scale);
	}

	glm::mat4 Node::GetWorldTransform() const
	{
		if (parent_) {
			return parent_->GetWorldTransform() * GetLocalTransform();
		}
		return GetLocalTransform();
	}

	void Node::SetWorldTransform(const glm::mat4& transform)
	{
		glm::mat4 inv_parent_transform{ parent_ ? glm::inverse(parent_->GetWorldTransform()) : glm::mat4(1.0f) };
		SetLocalTransform(inv_parent_transform * transform);
	}

	void Node::SetLocalTransform(const glm::mat4& transform)
	{
		glm::vec3 v1{ glm::vec3(transform[0]) };
		glm::vec3 v2{ glm::vec3(transform[1]) };
		glm::vec3 v3{ glm::vec3(transform[2]) };
		float l1{ glm::length(v1) };
		float l2{ glm::length(v2) };
		float l3{ glm::length(v3) };
		glm::mat3 rot_mat{ v1 / l1, v2 / l2, v3 / l3 };

		scale = { l1, l2, l3 };
		position = glm::vec3(transform[3]);
		rotation = glm::quat_cast(rot_mat);
	}

	bool Node::HasAncestor(Node* node)
	{
		if (parent_) {
			return (parent_ == node) || parent_->HasAncestor(node);
		}
		return false;
	}

	void Scene::Initialize(renderer::VulkanRenderer* renderer)
	{
		renderer_ = renderer;

		// Every node will be a descendent of the root node.
		root_node_ = CreateNode();
	}

	void Scene::CleanUp()
	{
		for (Node* node : nodes_) {
			delete node;
		}
		nodes_.clear();
		root_node_ = nullptr;
	}

	void Scene::ImportGLTF(const std::filesystem::path& path, std::vector<std::string>* out_node_names, std::vector<std::string>* out_material_names)
	{
		logger::Print("Loading glTF file: %s\n", path.string().c_str());

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string().c_str());
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

		uint32_t mesh_starting_index{ renderer_->MeshCount() };
		uint32_t material_starting_index{ (uint32_t)renderer_->GetMaterials().size() };
		std::vector<int> duplicate_indices{ renderer_->LoadMeshesAndMaterialsGLTF(model, out_material_names) };

		int starting_index{ (int)nodes_.size() };
		nodes_.reserve(starting_index + model.nodes.size());

		for (tinygltf::Node gltf_node : model.nodes)
		{
			if (out_node_names) {
				out_node_names->push_back(gltf_node.name);
			}

			Node* node{ CreateNode() };

			if (gltf_node.mesh >= 0)
			{
				std::vector<int> material_indices{};
				for (tinygltf::Primitive& primitive : model.meshes[gltf_node.mesh].primitives)
				{
					if (primitive.material != -1) {
						material_indices.push_back(material_starting_index + primitive.material);
					}
					else {
						material_indices.push_back(material_starting_index);
					}
				}

				// If not -1, this indicates this mesh has been loaded already, so we give the render object the index of the existing mesh.
				uint32_t mesh_idx{ duplicate_indices[gltf_node.mesh] == -1 ? mesh_starting_index + gltf_node.mesh : (uint32_t)duplicate_indices[gltf_node.mesh] };
				AddRenderObjectToNode(node, renderer_->CreateRenderObject(mesh_idx, material_indices));

				if (!gltf_node.translation.empty()) {
					node->position = glm::vec3{ (float)gltf_node.translation[0], (float)gltf_node.translation[1], (float)gltf_node.translation[2] };
				}

				if (!gltf_node.scale.empty()) {
					node->scale = glm::vec3{ (float)gltf_node.scale[0], (float)gltf_node.scale[1], (float)gltf_node.scale[2] };
				}

				if (!gltf_node.rotation.empty()) {
					node->rotation = glm::quat{ (float)gltf_node.rotation[3], (float)gltf_node.rotation[0], (float)gltf_node.rotation[1], (float)gltf_node.rotation[2] };
				}
				else {
					node->rotation = glm::quat{ 1.0f, 0.0f, 0.f, 0.0f };
				}

				for (int child_idx : gltf_node.children) {
					node->AddChild(nodes_[starting_index + child_idx]);
				}
			}
		}
		renderer_->UpdateMaterials();
	}

	void Scene::GenerateParticlesOnNode(Node* node)
	{
		if (node->render_object == renderer::NULL_HANDLE) {
			AddRenderObjectToNode(node, renderer_->CreateBlankRenderObject());
		}

		renderer_->InvokeParticleGenShader(node->render_object);
		renderer_->UpdateMaterials();
	}

	void Scene::PlayParticleSimulation()
	{
		renderer_->PlayParticleSimulation();
	}

	void Scene::UploadRenderObjectsRec(Node* root, const glm::mat4& parent_transform)
	{
		glm::mat4 local_transform{ root->GetLocalTransform() };
		// We do not use GetWorldTransform() here since it's more efficient to accumulate the transform down the tree,
		// rather than needing to recurse back up at each step to get the world transform.
		glm::mat4 world_transform{ parent_transform * local_transform };

		// Not every node has a render object.
		if (root->render_object != renderer::NULL_HANDLE) {
			renderer_->SetRenderObjectTransform(root->render_object, world_transform);
		}

		for (Node* child : root->children_) {
			UploadRenderObjectsRec(child, world_transform);
		}
	}

	void Scene::UploadRenderObjects()
	{
		UploadRenderObjectsRec(root_node_, glm::mat4(1.0f));
	}

	void Scene::UploadCamera()
	{
		renderer_->SetCameraMatrix(camera_.GetViewMatrix(), camera_.GetProjectionMatrix(renderer_->GetViewportExtent()));
	}

	Camera& Scene::GetCamera()
	{
		return camera_;
	}

	Node* Scene::CreateNode(uint32_t id)
	{
		Node* node_ptr{ new Node(id) };
		node_ptr->SetParent(root_node_);
		nodes_.push_back(node_ptr);
		return node_ptr;
	}

	Node* Scene::CreateNode()
	{
		return CreateNode(next_node_id_++);
	}

	Node* Scene::CreateNodeFromID(uint32_t id)
	{
		return CreateNode(id);
	}

	void Scene::SetNextNodeID(uint32_t id)
	{
		next_node_id_ = id;
	}

	std::vector<Node*>& Scene::GetNodes()
	{
		return nodes_;
	}

	Node* Scene::GetRootNode() const
	{
		return root_node_;
	}

	Node* Scene::GetNodeByRenderObject(renderer::RenderObjectHandle handle)
	{
		auto it{ render_object_node_map_.find(handle) };
		if (it != render_object_node_map_.end()) {
			return it->second;
		}
		return nullptr;
	}

	void Scene::AddRenderObjectToNode(Node* node, renderer::RenderObjectHandle handle)
	{
		node->render_object = handle;

		if (node->render_object != renderer::NULL_HANDLE) {
			render_object_node_map_[node->render_object] = node;
		}
	}

	glm::mat4 Camera::GetViewMatrix() const
	{
		return glm::inverse(glm::translate(position) * glm::toMat4(rotation));
	}

	glm::mat4 Camera::GetProjectionMatrix(const renderer::Extent& viewport_extent) const
	{
		glm::mat4 projection{ glm::infinitePerspective(glm::radians(fov), viewport_extent.width / (float)viewport_extent.height, near_plane) };
		projection[1][1] *= -1; // Vulkan's y-axis is opposite that of OpenGl's.
		return projection;
	}

	glm::mat4 Camera::GetProjectionViewMatrix(const renderer::Extent& viewport_extent) const
	{
		return GetProjectionMatrix(viewport_extent) * GetViewMatrix();
	}
}
