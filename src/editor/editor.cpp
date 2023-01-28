#include "editor.h"

#include <string>
#include <math.h>
#include <fstream>
#include "imgui.h"
#include "glm/gtx/vector_angle.hpp"
#include "glm/gtx/quaternion.hpp"

#include "gui.h"

const std::string ROOT_NODE_NAME{ "__root__" };

namespace jsonkey
{
	const std::string MAX_NODE_ID{ "max_node_id" };

	const std::string NODES{ "nodes" };
	// Nodes members.
	const std::string NAME{ "name" };
	const std::string ID{ "id" };
	const std::string POSITION{ "position" };
	const std::string SCALE{ "scale" };
	const std::string ROTATION{ "rotation" };
	const std::string RENDER_OBJECT{ "render_object" };
	const std::string PARENT_ID{ "parent_id" };
	const std::string CHILDREN_IDS{ "children_ids" };
	// End nodes members.

	const std::string VIEWPORT_CAMERA{ "viewport_camera" };
	// Camera members.
	const std::string VIEWPORT_FOCAL_DISTANCE{ "viewport_focal_distance" };
	const std::string VIEWPORT_FOCAL_POINT{ "viewport_focal_point" };
	const std::string VIEWPORT_THETA{ "viewport_theta" };
	const std::string VIEWPORT_PHI{ "viewport_phi" };
	const std::string VIEWPORT_FOV{ "viewport_fov" };
	const std::string VIEWPORT_SPEED{ "viewport_speed" };
	// End camera members.

	const std::string MATERIALS{ "materials" };
	// Begin editor material members.
	const std::string MATERIAL_NAME{ "name" };
	// End editor material members.

	// Editor settings.
	const std::string SETTINGS_PROJECT_DIRECTORIES_PATH{ "project_directories_path" };

}

void InitializationCallback(void* user_data)
{
	Editor* editor{ (Editor*)user_data };
	editor->InitializeGui();
}

void Editor::Initialize(pmk::Pumpkin* pumpkin)
{
	pumpkin_ = pumpkin;
	auto& scene{ pumpkin->GetScene() };
	controller_.Initialize(&scene.GetCamera());

	// Set editor root node to match Pumpkin's scene root node.
	node_map_[scene.GetRootNode()->node_id] = new EditorNode{ scene.GetRootNode(), ROOT_NODE_NAME };
	root_node_ = node_map_[scene.GetRootNode()->node_id];

	LoadEditorSettings();

	gui_.Initialize(this);
}

void Editor::CleanUp()
{
	for (auto& pair : node_map_) {
		delete pair.second;
	}
	node_map_.clear();

	gui_.CleanUp();
}

// We pass rendered_image_id to the draw callback instead of at initialization because the
// rendered image ID is a frame resource, so we alternate which one gets drawn to each frame.
// It is a pointer because we may need to change the underlying descriptor set if the
// viewport is resized.
void GuiCallback(ImTextureID* rendered_image_id, void* user_data)
{
	Editor* editor{ (Editor*)user_data };
	editor->DrawGui(rendered_image_id);
}

void Editor::InitializeGui()
{
	gui_.InitializeGui();
}

void Editor::DrawGui(ImTextureID* rendered_image_id)
{
	gui_.DrawGui(rendered_image_id);
}

renderer::ImGuiCallbacks Editor::GetEditorInfo()
{
	return {
		.initialization_callback = InitializationCallback,
		.gui_callback = GuiCallback,
		.user_data = (void*)this,
	};
}

CameraController& Editor::GetCameraController()
{
	return controller_;
}

pmk::Pumpkin* Editor::GetPumpkin()
{
	return pumpkin_;
}

EditorNode* Editor::GetRootNode() const
{
	return root_node_;
}

std::unordered_map<uint32_t, EditorNode*>& Editor::GetNodeMap()
{
	return node_map_;
}

void Editor::SetNodeSelection(EditorNode* node, bool selected)
{
	if (selected) {
		SelectNode(node);
	}
	else {
		DeselectNode(node);
	}
}

void Editor::ToggleNodeSelection(EditorNode* node)
{
	SetNodeSelection(node, !IsNodeSelected(node));
}

void Editor::SelectNode(EditorNode* node)
{
	if (!multi_select_enabled_) {
		selected_nodes_.clear();
	}

	selected_nodes_.insert(node);
	active_selection_node_ = node;
}

void Editor::DeselectNode(EditorNode* node)
{
	selected_nodes_.erase(node);
	if (active_selection_node_ == node) {
		active_selection_node_ = nullptr;
	}
}

bool Editor::IsNodeSelected(EditorNode* node)
{
	return selected_nodes_.find(node) != selected_nodes_.end();
}

bool Editor::SelectionEmpty() const
{
	return selected_nodes_.empty();
}

void Editor::NodeClicked(EditorNode* node)
{
	if (multi_select_enabled_) {
		ToggleNodeSelection(node);
	}
	else {
		SelectNode(node);
	}
}

void Editor::FileClicked(const std::filesystem::path& path)
{
	if (multi_select_enabled_) {
		ToggleFileSelection(path);
	}
	else {
		SelectFile(path);
	}
}

void Editor::FileDoubleClicked(const std::filesystem::path& path)
{
	if (path.extension() == ".gltf") {
		ImportGLTF(path);
	}
	else {
		logger::Print("Unrecognized file format: %s\n", path.extension().string().c_str());
	}
}

bool Editor::IsFileSelected(const std::filesystem::path& path)
{
	return selected_files_.find(path) != selected_files_.end();
}

void Editor::SetFileSelection(const std::filesystem::path& path, bool selected)
{
	if (selected) {
		SelectFile(path);
	}
	else {
		DeselectFile(path);
	}
}

void Editor::ToggleFileSelection(const std::filesystem::path& path)
{
	SetFileSelection(path, !IsFileSelected(path));
}

void Editor::SelectFile(const std::filesystem::path& path)
{
	if (!multi_select_enabled_) {
		selected_files_.clear();
	}

	selected_files_.insert(path);
	active_selection_file_ = path;
}

void Editor::DeselectFile(const std::filesystem::path& path)
{
	selected_files_.erase(path);
	if (active_selection_file_ == path) {
		active_selection_file_ = "";
	}
}

std::filesystem::path Editor::GetProjectDirectory() const
{
	return project_directory_;
}

void Editor::SaveProject() const
{
	nlohmann::json j{};
	uint32_t max_node_id{ 0 };

	for (auto& pair : node_map_)
	{
		EditorNode* node{ pair.second };
		std::string json_node_name{ "node" };
		json_node_name += std::to_string(node->node->node_id);

		glm::vec3& p{ node->node->position };
		glm::vec3& s{ node->node->scale };
		glm::quat& q{ node->node->rotation };
		pmk::Node* parent{ node->node->GetParent() };

		j[jsonkey::NODES] += {
			{ jsonkey::NAME, node->GetName() },
			{ jsonkey::ID, node->node->node_id },
			{ jsonkey::POSITION, { p.x, p.y, p.z } },
			{ jsonkey::SCALE, { s.x, s.y, s.z } },
			{ jsonkey::ROTATION, { q.x, q.y, q.z, q.w } },
			{ jsonkey::RENDER_OBJECT, node->node->render_object },
			{ jsonkey::PARENT_ID, parent ? parent->node_id : std::numeric_limits<uint32_t>::max() },
			{ jsonkey::CHILDREN_IDS, node->node->GetChildrenIDs() },
		};

		if (node->node->node_id > max_node_id) {
			max_node_id = node->node->node_id;
		}
	}

	{
		const glm::vec3& p{ controller_.GetFocalPoint() };

		j[jsonkey::VIEWPORT_CAMERA] = {
			{ jsonkey::VIEWPORT_FOCAL_POINT, {p.x, p.y, p.z} },
			{ jsonkey::VIEWPORT_FOCAL_DISTANCE, controller_.GetFocalDistance() },
			{ jsonkey::VIEWPORT_THETA, controller_.GetTheta() },
			{ jsonkey::VIEWPORT_PHI, controller_.GetPhi() },
			{ jsonkey::VIEWPORT_FOV, controller_.GetCamera()->fov },
			{ jsonkey::VIEWPORT_SPEED, controller_.GetMovementSpeed() },
		};
	}

	j[jsonkey::MAX_NODE_ID] = max_node_id;

	auto project_data_path{ project_directory_ / PROJECT_DATA_RELATIVE_PATH };
	std::filesystem::create_directories(project_data_path); // Make the directory if it doesn't exist.

	pumpkin_->DumpRenderData(j, project_data_path / VERTEX_DATA_FILE_NAME, project_data_path / INDEX_DATA_FILE_NAME);

	// The other material properties are saved in DumpRenderData() but the material name is specific to the editor so it's saved here.
	uint32_t mat_idx{ 0 };
	for (auto& json_material : j[jsonkey::MATERIALS]) {
		json_material[jsonkey::MATERIAL_NAME] = materials_[mat_idx++]->GetName();
	}

	auto json_path{ project_data_path / PROJECT_DATA_JSON_NAME };
	logger::Print("Saving json to %s\n", json_path.string().c_str());

	std::ofstream o{ json_path };
	std::string dump = j.dump();
	o << std::setw(4) << j << '\n';
	o.close();
}

void Editor::NewProject(const std::filesystem::path& proj_dir)
{
	std::filesystem::create_directories(proj_dir / ASSETS_RELATIVE_PATH); // Make the directory if it doesn't exist.
	project_directory_ = proj_dir;
	SaveProject(); // Save so the empty project can be loaded again if user doesn't ever save this project.
}

void Editor::LoadProject(const std::filesystem::path& proj_dir)
{
	if (!project_directory_.empty()) {
		//ResetProject();
	}

	project_directory_ = proj_dir;
	auto project_data_path{ project_directory_ / PROJECT_DATA_RELATIVE_PATH };
	std::ifstream f(project_data_path / PROJECT_DATA_JSON_NAME);
	nlohmann::json j{ nlohmann::json::parse(f) };

	std::vector<renderer::Material*>& materials{ pumpkin_->GetMaterials() };
	const uint32_t material_start_idx{ (uint32_t)materials.size() }; // Will be index of first newly loaded material.
	std::vector<int> material_indices{};

	LoadNodeData(j);
	pumpkin_->LoadRenderData(j, project_data_path / VERTEX_DATA_FILE_NAME, project_data_path / INDEX_DATA_FILE_NAME, &material_indices);

	// Load viewport camera.
	{
		auto& p{ j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_FOCAL_POINT] };

		controller_.SetFocalPoint({ p[0], p[1], p[2] });
		controller_.SetFocalDistance(j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_FOCAL_DISTANCE]);
		controller_.SetTheta(j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_THETA]);
		controller_.SetPhi(j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_PHI]);
		controller_.GetCamera()->fov = j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_FOV];
		controller_.GetMovementSpeed() = j[jsonkey::VIEWPORT_CAMERA][jsonkey::VIEWPORT_SPEED];
	}

	// Create new editor materials.
	for (uint32_t i{ 0 }; material_start_idx + i < (uint32_t)materials.size(); ++i)
	{
		std::string material_name{ j[jsonkey::MATERIALS][i][jsonkey::MATERIAL_NAME] };
		materials_.push_back(new EditorMaterial{ materials[material_start_idx + i], material_name });
	}

	// Update the new materials user count.
	for (int i : material_indices) {
		++materials_[material_start_idx + i]->user_count;
	}
}

void Editor::LoadNodeData(const nlohmann::json& j)
{
	// Load basic node data.
	for (auto& json_node : j[jsonkey::NODES])
	{
		if (json_node[jsonkey::ID] == 0) {
			continue;
		}

		pmk::Node* node{ pumpkin_->GetScene().CreateNodeFromID(json_node[jsonkey::ID]) };
		node_map_[node->node_id] = new EditorNode{ node, json_node[jsonkey::NAME] };

		auto& p{ json_node[jsonkey::POSITION] };
		auto& s{ json_node[jsonkey::SCALE] };
		auto& q{ json_node[jsonkey::ROTATION] };

		node->render_object = json_node[jsonkey::RENDER_OBJECT];
		node->position = glm::vec3{ p[0], p[1], p[2] };
		node->scale = glm::vec3{ s[0], s[1], s[2] };
		node->rotation = glm::quat{ q[3], q[0], q[1], q[2] };
	}
	pumpkin_->GetScene().SetNextNodeID(j[jsonkey::MAX_NODE_ID] + 1);

	// Set node parent/child data.
	for (auto& json_node : j[jsonkey::NODES])
	{
		if (json_node[jsonkey::PARENT_ID] == std::numeric_limits<uint32_t>::max()) {
			continue;
		}

		pmk::Node* node{ node_map_[json_node[jsonkey::ID]]->node };
		pmk::Node* parent{ node_map_[json_node[jsonkey::PARENT_ID]]->node };
		node->SetParent(parent); // Parent's children are set automatically from this.
	}
}

EditorNode* Editor::NodeToEditorNode(pmk::Node* node)
{
	return node_map_[node->node_id];
}

void Editor::ImportGLTF(const std::filesystem::path& path)
{
	std::vector<pmk::Node*>& nodes{ pumpkin_->GetScene().GetNodes() };
	std::vector<renderer::Material*>& materials{ pumpkin_->GetMaterials() };

	// The starting index before we add more nodes.
	uint32_t node_idx{ (uint32_t)nodes.size() };
	uint32_t mat_idx{ (uint32_t)materials.size() };

	// We use an out variable for names since the Pumpkin project doesn't know about the editor or EditorNode.
	std::vector<std::string> node_names;
	std::vector<std::string> material_names;

	// Add new nodes to scene.
	pumpkin_->GetScene().ImportGLTF(path, &node_names, &material_names);

	// Make a wrapper EditorMaterial for each imported renderer::Material.
	uint32_t name_index{ 0 };
	while (mat_idx < materials.size())
	{
		materials_.push_back(new EditorMaterial{ materials[mat_idx], material_names[name_index] });
		++mat_idx;
		++name_index;
	}

	// Make a wrapper EditorNode for each imported pmk::Node.
	name_index = 0;
	while (node_idx < nodes.size())
	{
		EditorNode* editor_node{ new EditorNode{ nodes[node_idx], node_names[name_index] } };
		node_map_[nodes[node_idx]->node_id] = editor_node;

		for (int material_index : GetMaterialIndicesFromNode(editor_node)) {
			++materials_[material_index]->user_count;
		}

		++node_idx;
		++name_index;
	}
}

void Editor::SetMultiselect(bool multiselect)
{
	multi_select_enabled_ = multiselect;
}

EditorNode* Editor::GetActiveSelectionNode()
{
	return active_selection_node_;
}

TransformType Editor::GetActiveTransformType() const
{
	return transform_info_.type;
}

void Editor::SetActiveTransformType(TransformType state)
{
	transform_info_ = TransformInfo{}; // Reset transform info.
	transform_info_.type = state;

	if (state != TransformType::NONE) {
		CacheOriginalTransforms();
	}
}

void Editor::SetTransformLock(TransformLockFlags lock)
{
	transform_info_.lock = lock;
}

void Editor::CacheOriginalTransforms()
{
	transform_info_.original_transforms.clear();
	transform_info_.average_start_pos = GetSelectedNodesAveragePosition();

	for (EditorNode* node : selected_nodes_) {
		transform_info_.original_transforms[node] = Transform{ node->node->GetWorldPosition(), node->node->scale, node->node->GetWorldRotation() };
	}
}

void Editor::ProcessTranslationInput(const glm::vec2& mouse_delta)
{
	// Distance from the camera to the plane of the camera frustum that the node lies on.
	float node_plane_dist{ glm::dot(controller_.GetForward(), transform_info_.average_start_pos - controller_.GetCamera()->position) };
	// Distance to the plane of camera frustum with height of 1.
	float one_height_dist{ 1.0f / (2.0f * std::tanf(glm::radians(controller_.GetCamera()->fov / 2.0f))) };
	// This scales screen_delta to get world space offset that moves node specified ratio across screen.
	float movement_multiplier{ node_plane_dist / one_height_dist };

	glm::vec2 screen_displacement{ movement_multiplier * mouse_delta.x, -movement_multiplier * mouse_delta.y }; // Negate y since negative screenspace y means up.
	glm::vec3 world_displacement{ (glm::vec3)(controller_.GetCamera()->rotation * glm::vec4{screen_displacement, 0.0f, 1.0f}) };

	// Handle axis locking.
	world_displacement.x = (transform_info_.lock & TransformLockFlags::X) != TransformLockFlags::NONE ? 0.0f : world_displacement.x;
	world_displacement.y = (transform_info_.lock & TransformLockFlags::Y) != TransformLockFlags::NONE ? 0.0f : world_displacement.y;
	world_displacement.z = (transform_info_.lock & TransformLockFlags::Z) != TransformLockFlags::NONE ? 0.0f : world_displacement.z;

	for (EditorNode* node : selected_nodes_)
	{
		// If a node's ancestor is selected then transforming both parent and child will result in double the transform of the child.
		if (!IsNodeAncestorSelected(node)) {
			node->node->SetWorldPosition(transform_info_.original_transforms[node].position + world_displacement);
		}
	}
}

void Editor::ProcessRotationInput(const glm::vec2& mouse_pos)
{
	glm::vec2 rotation_center{ WorldToScreenSpace(transform_info_.average_start_pos) };
	glm::vec2 center_to_start{ transform_info_.mouse_start_pos - rotation_center };
	glm::vec2 center_to_current{ mouse_pos - rotation_center };

	float angle{ glm::orientedAngle(glm::normalize(center_to_start), glm::normalize(center_to_current)) };

	// Handle axis locking.
	glm::vec3 axis{ controller_.GetForward() };
	switch (transform_info_.lock)
	{
	case TransformLockFlags::Y | TransformLockFlags::Z: // Rotate about x.
		axis = glm::vec3{ 1.0f, 0.0f, 0.0f };
		break;
	case TransformLockFlags::X | TransformLockFlags::Z:  // Rotate about y.
		axis = glm::vec3{ 0.0f, 1.0f, 0.0f };
		break;
	case TransformLockFlags::X | TransformLockFlags::Y:  // Rotate about z.
		axis = glm::vec3{ 0.0f, 0.0f, 1.0f };
		break;
	}

	glm::quat rotation{ glm::angleAxis(angle, axis) };

	for (EditorNode* node : selected_nodes_)
	{
		// If a node's ancestor is selected then transforming both parent and child will result in double the transform of the child.
		if (!IsNodeAncestorSelected(node))
		{
			node->node->SetWorldRotation(rotation * transform_info_.original_transforms[node].rotation);

			// Note: We use saved average start position because a) it's more efficient than recalculating it since it shouldn't change.
			//       And b) the feedback loop accumulates floating point errors causing instability if we update the average position every frame.
			glm::vec3 new_position{ transform_info_.original_transforms[node].position };
			new_position -= transform_info_.average_start_pos; // Convert to local space where average_position is origin.
			new_position = rotation * new_position;            // Rotate about average_position.
			new_position += transform_info_.average_start_pos; // Restore position to world space.
			node->node->SetWorldPosition(new_position);
		}
	}
}

void Editor::ProcessScaleInput(const glm::vec2& mouse_pos)
{
	glm::vec2 rotation_center{ WorldToScreenSpace(transform_info_.average_start_pos) };
	float start_distance{ glm::distance(transform_info_.mouse_start_pos, rotation_center) };
	float current_distance{ glm::distance(mouse_pos, rotation_center) };

	float scale{ current_distance / start_distance };
	glm::vec3 scale_vec{
		(transform_info_.lock & TransformLockFlags::X) != TransformLockFlags::NONE ? 1.0f : scale,
		(transform_info_.lock & TransformLockFlags::Y) != TransformLockFlags::NONE ? 1.0f : scale,
		(transform_info_.lock & TransformLockFlags::Z) != TransformLockFlags::NONE ? 1.0f : scale,
	};

	for (EditorNode* node : selected_nodes_)
	{
		// If a node's ancestor is selected then transforming both parent and child will result in double the transform of the child.
		if (!IsNodeAncestorSelected(node))
		{
			node->node->scale = scale_vec * transform_info_.original_transforms[node].scale;

			glm::vec3 new_position{ transform_info_.original_transforms[node].position };
			new_position -= transform_info_.average_start_pos; // Convert to local space where average_position is origin.
			new_position = scale_vec * new_position;           // Scale about average_position.
			new_position += transform_info_.average_start_pos; // Restore position to world space.
			node->node->SetWorldPosition(new_position);
		}
	}
}

void Editor::ProcessTransformInput(const glm::vec2& mouse_pos)
{
	// Save starting position of mouse so we can calculate delta.
	// We save the mouse position here instead of during setting the active transform type, because we need mouse_pos.
	if (transform_info_.mouse_start_pos == glm::vec2{ -1.0f, -1.0f }) {
		transform_info_.mouse_start_pos = mouse_pos;
	}

	glm::vec2 mouse_delta{ mouse_pos - transform_info_.mouse_start_pos };

	switch (transform_info_.type)
	{
	case TransformType::TRANSLATE:
		ProcessTranslationInput(mouse_delta);
		break;

	case TransformType::ROTATE:
		ProcessRotationInput(mouse_pos);
		break;

	case TransformType::SCALE:
		ProcessScaleInput(mouse_pos);
		break;
	}
}

void Editor::ApplyTransformInput()
{
	SetActiveTransformType(TransformType::NONE);
}

void Editor::CancelTransformInput()
{
	for (EditorNode* node : selected_nodes_)
	{
		node->node->SetWorldPosition(transform_info_.original_transforms[node].position);
		node->node->scale = transform_info_.original_transforms[node].scale;
		node->node->SetWorldRotation(transform_info_.original_transforms[node].rotation);
	}

	SetActiveTransformType(TransformType::NONE);
}

const EditorGui& Editor::GetGui() const
{
	return gui_;
}

bool Editor::IsNodeAncestorSelected(EditorNode* node)
{
	pmk::Node* parent{ node->node->GetParent() };

	if (parent)
	{
		EditorNode* editor_parent{ NodeToEditorNode(parent) };
		return IsNodeSelected(editor_parent) || IsNodeAncestorSelected(editor_parent);
	}

	return false;
}

glm::vec3 Editor::GetSelectedNodesAveragePosition() const
{
	if (selected_nodes_.empty()) {
		return glm::vec3{};
	}

	glm::vec3 sum{};
	for (EditorNode* node : selected_nodes_) {
		sum += node->node->GetWorldPosition();
	}
	return sum / (float)selected_nodes_.size();
}

glm::vec2 Editor::WorldToScreenSpace(const glm::vec3& world_pos) const
{
	const renderer::Extent& viewport_extent{ gui_.GetViewportExtent() };
	glm::mat4 proj_view{ controller_.GetCamera()->GetProjectionViewMatrix(viewport_extent) };
	glm::vec4 projection{ proj_view * glm::vec4{world_pos, 1.0f} }; // Screen space center of rotation.
	glm::vec2 viewport_pos{ (glm::vec2)(projection / projection.w) };

	// Convert range of each component from [-1, 1] to [0, 1].
	viewport_pos = 0.5f * viewport_pos + 0.5f;
	// Adjust x range by viewport extent ratio.
	viewport_pos.x *= viewport_extent.width / (float)viewport_extent.height;

	return viewport_pos;
}

const std::vector<int>& Editor::GetMaterialIndicesFromNode(EditorNode* node)
{
	return pumpkin_->GetMaterialIndices(node->node->render_object);
}

void Editor::SetNodeMaterial(EditorNode* node, uint32_t geometry_index, int material_index)
{
	const std::vector<int>& material_indices{ GetMaterialIndicesFromNode(node) };
	--materials_[material_indices[geometry_index]]->user_count;
	++materials_[material_index]->user_count;

	pumpkin_->SetMaterialIndex(node->node->render_object, geometry_index, material_index);
}

uint32_t Editor::MakeMaterialUnique(int material_index)
{
	std::string old_name{ materials_[material_index]->GetName() };
	renderer::Material* material{ pumpkin_->MakeMaterialUnique(material_index) };
	materials_.push_back(new EditorMaterial{ material, old_name + "Copy" });
	return (uint32_t)(materials_.size() - 1);
}

std::filesystem::path GetAppDataDirectory()
{
	auto path{ std::filesystem::temp_directory_path().parent_path().parent_path().parent_path()};
	path /= "Roaming";
	path /= "Pumpkin";

	if (!std::filesystem::exists(path)) {
		std::filesystem::create_directories(path);
	}

	return path;
}

void Editor::LoadEditorSettings()
{
	std::filesystem::path app_data_dir{ GetAppDataDirectory() };
	auto settings_path{ app_data_dir / SETTINGS_FILE_NAME };

	// Load settings if possible, otherwise just use default settings.
	if (std::filesystem::exists(settings_path))
	{
		std::ifstream f(settings_path);
		nlohmann::json j{ nlohmann::json::parse(f) };

		std::string path_str{ j[jsonkey::SETTINGS_PROJECT_DIRECTORIES_PATH] };
		editor_settings.project_directories_path = std::filesystem::path{ path_str };

		f.close();
	}
	else
	{
		// Defaults.
		editor_settings.project_directories_path = "C:\\";
	}
}

void Editor::SaveEditorSettings()
{
	std::filesystem::path app_data_dir{ GetAppDataDirectory() };
	auto settings_path{ app_data_dir / SETTINGS_FILE_NAME };

	nlohmann::json j{};

	j[jsonkey::SETTINGS_PROJECT_DIRECTORIES_PATH] = editor_settings.project_directories_path.string();

	std::ofstream o{ settings_path };
	std::string dump = j.dump();
	o << std::setw(4) << j << '\n';
	o.close();

}

EditorNode::EditorNode(pmk::Node* pmk_node, const std::string& name)
	: node{ pmk_node }
	, name_buffer_{ new char[NAME_BUFFER_SIZE] {} }
{
	strcpy_s(name_buffer_, std::min(NAME_BUFFER_SIZE, (uint32_t)(name.size() + 1)), name.c_str());
}

EditorNode::EditorNode(pmk::Node* pmk_node)
	: EditorNode(pmk_node, std::string{ "Node " } + std::to_string(pmk_node->node_id))
{
}

EditorNode::~EditorNode()
{
	delete[] name_buffer_;
}

std::string EditorNode::GetName() const
{
	return std::string(name_buffer_);
}

char* EditorNode::GetNameBuffer() const
{
	return name_buffer_;
}

EditorMaterial::EditorMaterial(renderer::Material* pmk_material, const std::string& name)
	: material{ pmk_material }
	, name_buffer_{ new char[NAME_BUFFER_SIZE] {} }
{
	strcpy_s(name_buffer_, std::min(NAME_BUFFER_SIZE, (uint32_t)(name.size() + 1)), name.c_str());
}

EditorMaterial::EditorMaterial(renderer::Material* pmk_material)
	: EditorMaterial(pmk_material, std::string{ "Material" })
{
}

EditorMaterial::~EditorMaterial()
{
	delete[] name_buffer_;
}

std::string EditorMaterial::GetName() const
{
	return std::string(name_buffer_);
}

char* EditorMaterial::GetNameBuffer() const
{
	return name_buffer_;
}
