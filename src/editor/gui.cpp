#include "gui.h"

#include <cmath>
#include <string>
#include <climits>
#include <chrono>
#include <set>
#include <bit>
#include "imgui.h"
#include "glm/gtc/type_ptr.hpp"
#include "tinyfiledialogs.h"

#include "editor.h"
#include "pumpkin.h"
#include "logger.h"

constexpr uint32_t PROJECT_NAME_BUFFER_SIZE{ 16 };
constexpr uint32_t NODE_PROPERTY_ALIGNMENT{ 77 };
constexpr uint32_t SHADER_PROPERTY_ALIGNMENT{ 85 };
constexpr uint32_t PHYSICS_PROPERTY_ALIGNMENT{ 118 };

bool EditorNodeCmp::operator()(EditorNode* a, EditorNode* b) const
{
	return a->GetName() < b->GetName();
}

std::filesystem::path OpenFileDialog(const std::string& title, const std::filesystem::path& default_path, const std::vector<const char*>& filter_patterns, bool allow_multi_select)
{
	std::string default_path_str{ default_path.string() };
	char const* selection{ tinyfd_openFileDialog(title.c_str(), default_path_str.c_str(), (int)filter_patterns.size(), filter_patterns.data(), nullptr, (int)allow_multi_select) };
	return selection ? std::filesystem::path{ selection } : std::filesystem::path{};
}

std::filesystem::path SelectFolderDialog(const std::string& title, const std::filesystem::path& default_path)
{
	std::string default_path_str{ default_path.string() };
	const char* selection{ tinyfd_selectFolderDialog(title.c_str(), default_path_str.c_str()) };
	return selection ? std::filesystem::path{ selection } : std::filesystem::path{};
}

void ImGuiDarkTheme()
{
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
	colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(5.00f, 2.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.ScrollbarSize = 15;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 7;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;
}

void EditorGui::Initialize(Editor* editor)
{
	editor_ = editor;
	popup_name_buffer_ = new char[PROJECT_NAME_BUFFER_SIZE] {};
	project_popup_current_directory_ = editor_->editor_settings_.project_directories_path;
}

void EditorGui::CleanUp()
{
	delete[] popup_name_buffer_;
}

void EditorGui::InitializeGui()
{
	// Set ImGui settings.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = NULL; // Disable automatic saving of layout.
	io.IniSavingRate = 0.1f; // Small number so we can save often.
	io.WantCaptureKeyboard = true;

	std::string default_path_str{ editor_->GetDefaultLayoutPath().string() };
	ImGui::LoadIniSettingsFromDisk(default_path_str.c_str());
	ImGuiDarkTheme();
}

void EditorGui::DrawGui(ImTextureID* rendered_image_id)
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	MainMenu();
	TreeView();
	//ImGui::ShowDemoWindow();
	NodeProperties();
	Materials();
	Constraints();
	EngineViewport(rendered_image_id);
	FileBrowser();
	CameraControls();
	VoxelEditor();
	Debug();
	CheckProjectSelectionPopup();
}

void EditorGui::CheckProjectSelectionPopup()
{
	const char* popup_name{ "Open project" };

	if (open_project_selection_popup_) {
		ImGui::OpenPopup(popup_name);
		open_project_selection_popup_ = false;
	}

	if (ImGui::BeginPopupModal(popup_name))
	{
		ProjectSelectionPopup();
		ImGui::EndPopup();
	}
}

const renderer::Extent& EditorGui::GetViewportExtent() const
{
	return viewport_extent_;
}

const renderer::Extent& EditorGui::GetViewportWindowExtent() const
{
	return viewport_window_extent_;
}

void EditorGui::MainMenuSaveDefaultLayout()
{
	if (ImGui::MenuItem("Save as default layout"))
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantSaveIniSettings)
		{
			std::string default_path_str{ editor_->GetDefaultLayoutSaveLocation().string() };
			ImGui::SaveIniSettingsToDisk(default_path_str.c_str());
			logger::Print("Saved ImGui layout to disk.\n");
			io.WantSaveIniSettings = false;
		}
	}
}

void MainMenuSaveProject(Editor* editor)
{
	if (ImGui::MenuItem("Save project")) {
		editor->SaveProject();
	}
}

void EditorGui::MainMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			MainMenuSaveDefaultLayout();
			MainMenuSaveProject(editor_);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void EditorGui::DrawTreeNode(EditorNode* root, EditorNode** out_node_clicked)
{
	ImGuiTreeNodeFlags node_flags{ ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth };

	if (editor_->IsNodeSelected(root)) {
		node_flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool is_leaf{ root->node->GetChildren().empty() };

	if (is_leaf) {
		node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	bool node_open{ ImGui::TreeNodeEx((void*)(intptr_t)root->node->node_id, node_flags, root->GetName().c_str()) };

	if (!(*out_node_clicked) && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		*out_node_clicked = root;
	}

	if (!is_leaf && node_open)
	{
		for (EditorNode* node : GetSortedChildren(root)) {
			DrawTreeNode(node, out_node_clicked);
		}

		ImGui::TreePop();
	}
}

void EditorGui::TreeView()
{
	if (!ImGui::Begin("Tree view"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGuiTreeNodeFlags base_flags{ ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth };

	EditorNode* root_node{ editor_->GetRootNode() };
	auto& node_map{ editor_->GetNodeMap() };

	EditorNode* clicked_node{ nullptr };

	// We don't draw the root node itself, this just exists to make it easier to do operations to all nodes recursively.
	for (EditorNode* node : GetSortedChildren(root_node)) {
		DrawTreeNode(node, &clicked_node);
	}

	// If the defocused window is clicked in, it seems the IsWindowFocused is delayed a frame before it returns true.
	if (ImGui::IsWindowFocused() || clicked_node) {
		input_.ProcessTreeViewInput(editor_);
	}

	if (clicked_node) {
		editor_->NodeClicked(clicked_node);
	}

	ImGui::End();
}

bool EditorGui::MaterialTextureProperty(const std::string& name, bool* show_tex_ui, uint32_t* texture_index, bool* mat_changed, bool color_data)
{
	bool show_texture_ui{ true };
	ImGui::PushID(name.c_str());

	ImGui::Text(name.c_str());
	ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);

	bool has_texture{ *texture_index != NULL_INDEX };
	*show_tex_ui |= has_texture;

	if (!(*show_tex_ui) && ImGui::Button("Tex")) {
		*show_tex_ui = true;
	}
	else if (*show_tex_ui)
	{
		if (ImGui::BeginCombo("##TextureCombo", nullptr, ImGuiComboFlags_NoPreview))
		{
			for (uint32_t tex_idx = 0; tex_idx < (uint32_t)editor_->textures_.size(); ++tex_idx)
			{
				const bool is_selected{ *texture_index == tex_idx };
				if (ImGui::Selectable(editor_->textures_[tex_idx]->GetNameBuffer(), is_selected))
				{
					*texture_index = tex_idx;
					editor_->GetPumpkin()->UpdateMaterials();
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			auto selection = OpenFileDialog("Select texture", texture_popup_current_directory_, { "*.png", "*.jpg", "*.jpeg", "*.tga", "*.bmp", "*.psd", "*.gif" }, false);
			if (!selection.empty())
			{
				if (selection.has_root_directory()) {
					texture_popup_current_directory_ = selection.root_directory();
				}

				uint32_t tex_index{ editor_->ImportTexture(selection, color_data) };
				*texture_index = tex_index;
				*mat_changed = true;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("X"))
		{
			*texture_index = NULL_INDEX;
			*show_tex_ui = false;
			editor_->GetPumpkin()->UpdateMaterials();
		}

		ImGui::SameLine();
		ImGui::Text(*texture_index == NULL_INDEX ? "No texture selected." : editor_->GetTexture(*texture_index)->GetNameBuffer());
	}
	else
	{
		ImGui::SameLine();
		show_texture_ui = false;
	}

	ImGui::PopID();
	return show_texture_ui;
}

bool EditorGui::ShaderProperty(const std::string& name, uint32_t* shader_index, bool* compile_error)
{
	bool show_texture_ui{ true };
	bool shader_idx_changed{ false };
	ImGui::PushID(name.c_str());

	ImGui::Text(name.c_str());
	ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);

	if (ImGui::BeginCombo("##ShaderCombo", nullptr, ImGuiComboFlags_NoPreview))
	{
		for (uint32_t idx = 0; idx < (uint32_t)editor_->shaders_.size(); ++idx)
		{
			const bool is_selected{ *shader_index == idx };
			if (ImGui::Selectable(editor_->shaders_[idx]->GetNameBuffer(), is_selected))
			{
				*shader_index = idx;
				shader_idx_changed = true;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		auto selection = OpenFileDialog("Select shader", shader_popup_current_directory_, { "*.comp", "*.cs", "*.glsl" }, false);
		if (!selection.empty())
		{
			if (selection.has_root_directory()) {
				shader_popup_current_directory_ = selection.root_directory();
			}

			uint32_t idx{ editor_->ImportShader(selection) };
			*shader_index = idx;
			*compile_error = (idx == NULL_INDEX);

			if (!*compile_error) {
				shader_idx_changed = true;
			}
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("X")) {
		*shader_index = NULL_INDEX;
	}

	ImGui::SameLine();
	if (*compile_error) {
		ImGui::TextColored(ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f }, "Compilation failed.");
	}
	else {
		ImGui::Text(*shader_index == NULL_INDEX ? "No shader selected." : editor_->GetShader(*shader_index)->GetNameBuffer());
	}

	ImGui::PopID();
	return shader_idx_changed;
}

void EditorGui::NodeProperties()
{
	if (!ImGui::Begin("Node properties"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	EditorNode* active_node{ editor_->active_selection_node_ };

	if (active_node)
	{
		ImGui::Text("Transform");

		ImGui::Text("Node name");
		ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
		ImGui::InputText("##Node name", active_node->GetNameBuffer(), NAME_BUFFER_SIZE);

		ImGui::Text("Position");
		ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
		ImGui::DragFloat3("##Position", glm::value_ptr(active_node->node->position), 0.1f);

		ImGui::Text("Scale");
		ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
		ImGui::DragFloat3("##Scale", glm::value_ptr(active_node->node->scale), 0.1f);

		auto& rot{ active_node->node->rotation };
		ImGui::Text("Rotation");
		ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
		ImGui::Text("%.2f  %.2f  %.2f  %.2f", rot.x, rot.y, rot.z, rot.w);
		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

		RenderMaterials(active_node);

		// Rigid body gui.
		pmk::RigidBody* rb{ active_node->node->rigid_body };
		if (rb)
		{
			ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
			ImGui::Separator();
			ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

			ImGui::Text("Rigid body");

			ImGui::Text("Immovable");
			ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
			ImGui::Checkbox("##Immovable", &rb->immovable);

			ImGui::Text("Mass");
			ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
			ImGui::DragFloat("##RigidBodyMass", &rb->mass, 0.01f);

			ImGui::Text("Velocity");
			ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
			ImGui::DragFloat3("##Velocity", glm::value_ptr(rb->velocity), 0.1f);

			ImGui::Text("Angular vel");
			ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
			ImGui::DragFloat3("##AngularVelocity", glm::value_ptr(rb->angular_velocity), 0.1f);

			ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
			ImGui::Text("Debug rendering");
		}
	}
	else {
		ImGui::Text("No actively selected node.");
	}

	ImGui::End();
}

void EditorGui::RenderMaterials(EditorNode* node)
{
	if (node && node->node->render_object == renderer::NULL_HANDLE) {
		return;
	}

	std::vector<int> node_materials{};
	if (node) {
		node_materials = editor_->GetMaterialIndicesFromNode(node);
	}
	else
	{
		node_materials.resize(editor_->materials_.size());
		std::iota(node_materials.begin(), node_materials.end(), 0);
	}

	std::vector<const char*> node_materials_strings(node_materials.size());
	std::transform(node_materials.begin(), node_materials.end(), node_materials_strings.begin(),
		[=](int mat_idx) { return editor_->materials_[mat_idx]->GetNameBuffer(); });

	if (material_selected_geometry_index_ >= node_materials.size()) {
		material_selected_geometry_index_ = 0;
	}

	ImGui::Text("Render material");
	ImGui::ListBox("##MaterialList", &material_selected_geometry_index_, node_materials_strings.data(), (int)node_materials_strings.size(), 4);


	// We can only add/remove materials if we aren't looking at materials of specific node.
	if (!node)
	{
		ImGui::SameLine();
		ImGui::BeginGroup();

		// Add new material.
		if (ImGui::Button("+")) {
			material_selected_geometry_index_ = (int)editor_->NewMaterial();
		}

		// Delete selected material.
		bool selected_idx_in_range{ material_selected_geometry_index_ >= 0 && material_selected_geometry_index_ < (int)node_materials_strings.size() };
		ImGui::BeginDisabled(node_materials.size() <= 1 || !selected_idx_in_range);
		if (ImGui::Button("-"))
		{
			int selected_idx{ node_materials[material_selected_geometry_index_] };
			editor_->DeleteMaterial((uint32_t)selected_idx);
			material_selected_geometry_index_ = -1;
		}
		ImGui::EndDisabled();

		ImGui::EndGroup();
	}

	if (material_selected_geometry_index_ >= 0 && material_selected_geometry_index_ < (int)node_materials_strings.size())
	{
		EditorMaterial* mat{ editor_->materials_[node_materials[material_selected_geometry_index_]] };

		// Combo box to swap selected material for a different existing material.
		int material_selected_combo = node_materials[material_selected_geometry_index_];
		if (node)
		{
			if (ImGui::BeginCombo("##MaterialCombo", nullptr, ImGuiComboFlags_NoPreview))
			{
				for (uint32_t mat_idx = 0; mat_idx < (uint32_t)editor_->materials_.size(); ++mat_idx)
				{
					const bool is_selected{ material_selected_combo == mat_idx };
					if (node && ImGui::Selectable(editor_->materials_[mat_idx]->GetNameBuffer(), is_selected)) {
						editor_->SetNodeMaterial(node, material_selected_geometry_index_, mat_idx);
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
		}
		ImGui::InputText("##MaterialName", mat->GetNameBuffer(), NAME_BUFFER_SIZE);

		if (mat->user_count > 1)
		{
			ImGui::SameLine();
			std::string user_count_string{ std::to_string(mat->user_count) };

			ImGui::BeginDisabled(node == nullptr);
			if (ImGui::Button(user_count_string.c_str()))
			{
				uint32_t mat_copy{ editor_->MakeMaterialUnique((uint32_t)material_selected_combo) };
				editor_->SetNodeMaterial(node, material_selected_geometry_index_, mat_copy);
			}
			ImGui::EndDisabled();
		}

		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

		bool mat_changed{ false };

		if (!MaterialTextureProperty("Color", &mat->show_color_tex_ui_, &mat->material->color_index, &mat_changed, true)) {
			mat_changed |= ImGui::ColorEdit3("##Color", glm::value_ptr(mat->material->color));
		}

		if (!MaterialTextureProperty("Metallic", &mat->show_metallic_tex_ui_, &mat->material->metallic_index, &mat_changed, false)) {
			mat_changed |= ImGui::DragFloat("##Metallic", &mat->material->metallic, 0.01f, 0.000f, 1.0f);
		}

		if (!MaterialTextureProperty("Roughness", &mat->show_roughness_tex_ui_, &mat->material->roughness_index, &mat_changed, false)) {
			mat_changed |= ImGui::DragFloat("##Roughness", &mat->material->roughness, 0.01f, 0.0f, 1.0f);
		}

		if (!MaterialTextureProperty("Emission", &mat->show_emission_tex_ui_, &mat->material->emission_index, &mat_changed, false)) {
			mat_changed |= ImGui::DragFloat("##Emission", &mat->material->emission, 0.01f, 0.0f, 1000.0f);
		}

		bool show_normal_texture_ui{ true };
		MaterialTextureProperty("Normal", &show_normal_texture_ui, &mat->material->normal_index, &mat_changed, false);

		ImGui::Text("IOR");
		ImGui::SameLine(NODE_PROPERTY_ALIGNMENT);
		mat_changed |= ImGui::DragFloat("##IOR", &mat->material->ior, 0.01f, 1.0f, 2.0f);

		if (mat_changed) {
			editor_->pumpkin_->UpdateMaterials();
		}
	}
}

void EditorGui::PhysicsMaterials()
{
	ImGui::Text("Physics material");
	if (editor_->materials_.empty())
	{
		ImGui::Dummy(ImVec2{ 0.0f, 10.0f }); // Spacing.
		ImGui::Text("You must create a render material\nbefore creating a physics material.");
		return;
	}

	ImGui::PushID("Physics");

	// Generate the strings with indices prepended.
	std::vector<std::string> materials_strings{};
	materials_strings.resize(editor_->physics_materials_.size());
	for (uint32_t i{ 0 }; i < (uint32_t)editor_->physics_materials_.size(); ++i) {
		materials_strings[i] = std::to_string(i) + ": " + editor_->physics_materials_[i]->GetName();
	}

	// Convert to C strings.
	std::vector<const char*> materials_c_strings(editor_->physics_materials_.size());
	std::transform(materials_strings.begin(), materials_strings.end(), materials_c_strings.begin(),
		[=](const std::string& s) { return s.c_str(); });

	if (physics_material_selected_index_ >= editor_->physics_materials_.size()) {
		physics_material_selected_index_ = 0;
	}

	ImGui::ListBox("##PhysicsMaterialList", &physics_material_selected_index_, materials_c_strings.data(), (int)materials_c_strings.size(), 4);

	ImGui::SameLine();
	ImGui::BeginGroup();

	// Add new material.
	if (ImGui::Button("+")) {
		physics_material_selected_index_ = (int)editor_->NewPhysicsMaterial();
	}

	// Delete selected material.
	bool selected_idx_in_range{ physics_material_selected_index_ >= 0 && physics_material_selected_index_ < (int)materials_strings.size() };
	ImGui::BeginDisabled(editor_->physics_materials_.size() <= 1 || !selected_idx_in_range);
	if (ImGui::Button("-"))
	{
		editor_->DeletePhysicsMaterial((uint32_t)physics_material_selected_index_);
		physics_material_selected_index_ = -1;
	}
	ImGui::EndDisabled();
	ImGui::EndGroup();

	if (physics_material_selected_index_ >= 0 && physics_material_selected_index_ < (int)materials_strings.size())
	{
		EditorPhysicsMaterial* mat{ editor_->physics_materials_[physics_material_selected_index_] };

		ImGui::InputText("##PhysicsMaterialName", mat->GetNameBuffer(), NAME_BUFFER_SIZE);
		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

		uint32_t physics_render_material_index{ editor_->GetPhysicsMaterialRender(physics_material_selected_index_) };
		ImGui::Text("Render material");
		ImGui::SameLine(PHYSICS_PROPERTY_ALIGNMENT);

		std::string display_name{ physics_render_material_index < (uint32_t)editor_->materials_.size() ? editor_->materials_[physics_render_material_index]->GetName() : "Invalid" };
		if (ImGui::BeginCombo("##RenderMaterialCombo", display_name.c_str(), 0))
		{
			for (uint32_t mat_idx = 0; mat_idx < (uint32_t)editor_->materials_.size(); ++mat_idx)
			{
				const bool is_selected{ physics_render_material_index == mat_idx };
				if (ImGui::Selectable(editor_->materials_[mat_idx]->GetNameBuffer(), is_selected)) {
					editor_->SetPhysicsMaterialRender(physics_material_selected_index_, mat_idx);
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// Multiselect list of constraints.
		uint32_t constraint_mask{ editor_->GetPhysicsMaterialConstraintsMask(physics_material_selected_index_) };
		uint32_t constraint_count{ (uint32_t)std::popcount(constraint_mask) };
		std::string multiselect_box_name{ std::to_string(constraint_count) + " constraints selected" };

		ImGui::Text("Active constraints");
		ImGui::SameLine(PHYSICS_PROPERTY_ALIGNMENT);
		if (ImGui::BeginCombo("##ActiveConstraintsCombo", multiselect_box_name.c_str(), 0))
		{
			for (uint32_t c_idx = 0; c_idx < (uint32_t)editor_->constraints_.size(); ++c_idx)
			{
				const bool is_selected{ (bool)(constraint_mask & (1 << c_idx)) };
				if (ImGui::Selectable(editor_->constraints_[c_idx]->GetNameBuffer(), is_selected, ImGuiSelectableFlags_DontClosePopups))
				{
					constraint_mask ^= (1 << c_idx);
					editor_->SetPhysicsMaterialConstraintMask(physics_material_selected_index_, constraint_mask);
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Text("Density");
		ImGui::SameLine(PHYSICS_PROPERTY_ALIGNMENT);
		ImGui::DragFloat("##Density", &mat->material->density, 0.01f);
	}
	ImGui::PopID();
}

void EditorGui::Materials()
{
	if (!ImGui::Begin("Materials"))
	{
		ImGui::End();
		return;
	}

	RenderMaterials(nullptr);
	ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
	ImGui::Separator();
	ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
	PhysicsMaterials();

	ImGui::End();
}

void EditorGui::Constraints()
{
	if (!ImGui::Begin("Constraints"))
	{
		ImGui::End();
		return;
	}

	ImGui::PushID("Constraints");
	ImGui::Text("Physics constraints");

	// Generate the strings with indices prepended.
	std::vector<std::string> constraints_strings{};
	constraints_strings.resize(editor_->constraints_.size());
	for (uint32_t i{ 0 }; i < (uint32_t)editor_->constraints_.size(); ++i) {
		constraints_strings[i] = std::to_string(i) + ": " + editor_->constraints_[i]->GetName();
	}

	// Convert to C strings.
	std::vector<const char*> constraints_c_strings(editor_->constraints_.size());
	std::transform(constraints_strings.begin(), constraints_strings.end(), constraints_c_strings.begin(),
		[=](const std::string& s) { return s.c_str(); });

	if (constraint_selected_index_ >= editor_->constraints_.size()) {
		constraint_selected_index_ = 0;
	}

	ImGui::ListBox("##ConstraintList", &constraint_selected_index_, constraints_c_strings.data(), (int)constraints_c_strings.size(), 4);

	ImGui::SameLine();
	ImGui::BeginGroup();

	// Add new material.
	if (ImGui::Button("+")) {
		constraint_selected_index_ = (int)editor_->NewConstraint();
	}

	// Delete selected material.
	bool selected_idx_in_range{ constraint_selected_index_ >= 0 && constraint_selected_index_ < (int)constraints_strings.size() };
	ImGui::BeginDisabled(editor_->constraints_.size() <= 1 || !selected_idx_in_range);
	if (ImGui::Button("-"))
	{
		editor_->DeletePhysicsMaterial((uint32_t)constraint_selected_index_);
		constraint_selected_index_ = -1;
	}
	ImGui::EndDisabled();
	ImGui::EndGroup();

	if (constraint_selected_index_ >= 0 && constraint_selected_index_ < (int)constraints_strings.size())
	{
		EditorConstraint* constraint{ editor_->constraints_[constraint_selected_index_] };

		ImGui::InputText("##ConstraintName", constraint->GetNameBuffer(), NAME_BUFFER_SIZE);
		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

		ConstraintType selected_constraint_type{ editor_->GetConstraintType(constraint_selected_index_) };
		const char* selected_constraint_name{ constraint_names[(uint32_t)selected_constraint_type].c_str() };
		ImGui::Text("Constraint");
		ImGui::SameLine(PHYSICS_PROPERTY_ALIGNMENT);
		if (ImGui::BeginCombo("##ConstraintCombo", selected_constraint_name, 0))
		{
			for (uint32_t constraint_idx = 0; constraint_idx < (uint32_t)ConstraintType::CONSTRAINT_COUNT; ++constraint_idx)
			{
				const bool is_selected{ constraint_idx == (uint32_t)selected_constraint_type };
				if (ImGui::Selectable(constraint_names[constraint_idx].c_str(), is_selected)) {
					editor_->SetConstraintType(constraint_selected_index_, (ConstraintType)constraint_idx);
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.

		bool constraint_changed{ false };
		std::vector<std::pair<float*, std::string>> physics_parameters{ editor_->pumpkin_->GetConstraintParameters(constraint_selected_index_) };
		for (std::pair<float*, std::string>& parameter : physics_parameters)
		{
			ImGui::Text(parameter.second.c_str());
			ImGui::SameLine(PHYSICS_PROPERTY_ALIGNMENT);
			std::string imgui_id{ "##" + parameter.second };
			constraint_changed |= ImGui::DragFloat(imgui_id.c_str(), parameter.first, 0.01f, 0.0f, 0.0f, "%.7f");
		}

		if (constraint_changed) {
			editor_->pumpkin_->ConstraintParametersMutated(constraint_selected_index_);
		}
	}
	ImGui::PopID();
	ImGui::End();
}

// The 3D scene rendered from Renderer.
void EditorGui::EngineViewport(ImTextureID* rendered_image_id)
{
	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	bool success{ ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar) };
	ImGui::PopStyleVar(3);

	UpdateViewportSize();

	if (!success)
	{
		ImGui::End();
		return;
	}

	if (pumpkin_proj_loaded_)
	{
		input_.ProcessViewportAllInput(editor_, viewport_extent_);

		if (ImGui::IsWindowFocused()) {
			input_.ProcessViewportFocusInput(editor_, viewport_extent_);
		}

		if ((viewport_extent_.width != 0) && (viewport_extent_.height != 0)) {
			ImGui::Image(*rendered_image_id, ImGui::GetContentRegionAvail());
		}
	}

	ImGui::End();
}

bool IsPumpkinProject(const std::filesystem::directory_entry& dir)
{
	auto pmk_dir = dir / PROJECT_DATA_RELATIVE_PATH;

	if (!std::filesystem::exists(pmk_dir) || !std::filesystem::is_directory(pmk_dir)) {
		return false;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(pmk_dir))
	{
		if (entry.path().filename() == PROJECT_DATA_JSON_NAME) {
			return true;
		}
	}
	return false;
}

void EditorGui::ProjectSelectionPopup()
{
	namespace fs = std::filesystem;

	ImGui::Text("New project");
	ImGui::InputText("Project name", popup_name_buffer_, PROJECT_NAME_BUFFER_SIZE);
	ImGui::BeginDisabled(popup_name_buffer_[0] == '\0'); // Disable empty string.
	if (ImGui::Button("Create project"))
	{
		auto project_dir{ project_popup_current_directory_ / popup_name_buffer_ };
		editor_->NewProject(project_dir);
		current_directory_ = project_dir / ASSETS_RELATIVE_PATH;
		texture_popup_current_directory_ = project_dir / ASSETS_RELATIVE_PATH;
		pumpkin_proj_loaded_ = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();

	ImGui::Text("\nLoad existing project");
	ImGui::PushID(1);
	if (ImGui::Button("Browse"))
	{
		auto selection{ SelectFolderDialog("Select folder", project_popup_current_directory_) };
		if (!selection.empty())
		{
			project_popup_current_directory_ = selection;
			editor_->editor_settings_.project_directories_path = project_popup_current_directory_;
			editor_->SaveEditorSettings();
		}
	}

	ImGui::SameLine();
	ImGui::Text(project_popup_current_directory_.string().c_str());
	ImGui::PopID();

	ImGui::BeginChild("Project selection child", ImVec2(ImGui::GetContentRegionAvail().x, 200), false, 0);

	if (fs::exists(project_popup_current_directory_) && fs::is_directory(project_popup_current_directory_))
	{
		int idx{ 0 };

		for (const fs::directory_entry& entry : fs::directory_iterator(project_popup_current_directory_))
		{
			if (!entry.is_directory()) {
				continue;
			}

			std::string filename{ entry.path().filename().string() };

			ImGui::PushID(idx);

			if (ImGui::Selectable(filename.c_str(), entry == popup_selected_file_, ImGuiSelectableFlags_DontClosePopups))
			{
				popup_selected_file_ = entry;
				pumpkin_proj_selected_ = IsPumpkinProject(entry);
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				if (IsPumpkinProject(entry))
				{
					LoadProject();
					ImGui::CloseCurrentPopup();
				}
				else
				{
					project_popup_current_directory_ = entry;
					popup_selected_file_ = {};
				}
			}

			ImGui::PopID();
			++idx;
		}
	}

	ImGui::EndChild();

	ImGui::BeginDisabled(!pumpkin_proj_selected_);
	if (ImGui::Button("Load project"))
	{
		LoadProject();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
}

void EditorGui::FileBrowser()
{
	if (!ImGui::Begin("File browser"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	namespace fs = std::filesystem;

	ImGui::PushID(0);
	if (ImGui::Button("Parent directory")) {
		current_directory_ = current_directory_.parent_path();
	}

	ImGui::SameLine();
	ImGui::Text(current_directory_.string().c_str());
	ImGui::PopID();

	ImGui::BeginChild("File browser child", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true, 0);

	if (fs::exists(current_directory_) && fs::is_directory(current_directory_))
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		ImVec2 file_button_size(120, 120);
		int idx{ 0 };

		for (const fs::directory_entry& entry : fs::directory_iterator(current_directory_))
		{
			std::string filename{ entry.path().filename().string() };

			ImGui::PushID(idx);
			float last_button_x2 = ImGui::GetItemRectMax().x;
			float next_button_x2 = last_button_x2 + style.ItemSpacing.x + file_button_size.x; // Expected position if next button was on same line.
			if (idx != 0 && next_button_x2 < window_visible_x2) {
				ImGui::SameLine();
			}

			if (ImGui::IsWindowFocused()) {
				input_.ProcessFileBrowserInput(editor_);
			}

			if (ImGui::Selectable(filename.c_str(), editor_->IsFileSelected(entry), 0, file_button_size)) {
				editor_->FileClicked(entry);
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				if (fs::is_directory(entry)) {
					current_directory_ = entry;
				}
				else {
					editor_->FileDoubleClicked(entry);
				}
			}

			ImGui::PopID();
			++idx;
		}
	}

	ImGui::EndChild();
	ImGui::End();
}

void EditorGui::CameraControls()
{
	if (!ImGui::Begin("Camera controls"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGui::SliderFloat("Speed", &editor_->GetCameraController().GetMovementSpeed(), MINIMUM_MOVEMENT_SPEED, 10000.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("FOV", &editor_->GetCameraController().GetCamera()->fov, 30.0f, 90.0f, "%.1f", ImGuiSliderFlags_None);

	ImGui::End();
}

void EditorGui::VoxelEditor()
{
	if (!ImGui::Begin("Voxel Editor"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Voxel shader");

	if (ShaderProperty("Generation", &gen_shader_index_, &gen_shader_compile_error_)) {
		editor_->SetParticleGenShader(gen_shader_index_);
	}

	if (gen_shader_index_ != NULL_INDEX)
	{
		if (editor_->GetShader(gen_shader_index_)->GetCustomUniformBuffer().DrawGui(SHADER_PROPERTY_ALIGNMENT)) {
			particle_count_ = editor_->UpdateParticleGenShaderCustomUBO();
		}

		ImGui::Dummy({});
		ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
		if (ImGui::Button("Generate voxels")) {
			particle_count_ = editor_->GenerateVoxels();
		}

		ImGui::Text("Voxels");
		ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
		ImGui::Text("%u", particle_count_);
	}

	if (!editor_->GetParticleSimulationEmpty())
	{
		ImGui::Dummy({});
		ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
		if (editor_->GetPhysicsSimulationEnabled())
		{
			if (ImGui::Button("Pause")) {
				editor_->PausePhysicsSimulation();
			}
		}
		else
		{
			if (ImGui::Button("Play ")) {
				editor_->PlayPhysicsSimulation();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			editor_->ResetPhysicsSimulation();
		}
	}


	ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
	ImGui::Text("Debug rendering");

	ImGui::Text("Show grid");
	ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
	if (ImGui::Checkbox("##ShowMPMGrid", &editor_->show_particle_grid_)) {
		editor_->UpdateParticleOverlayEnabled();
	}

	ImGui::Text("Use depth");
	ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
	if (ImGui::Checkbox("##UseDepth", &editor_->use_particle_depth_)) {
		editor_->UpdateParticleOverlayEnabled();
	}

	constexpr float combo_width{ 155.0f };
	constexpr float max_val_width{ 35.0f };

	ImGui::PushItemWidth(combo_width);
	const char* selected_particle_color_named{ particle_color_mode_names[(uint32_t)editor_->particle_color_mode_].c_str() };
	ImGui::Text("Voxels");
	ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
	if (ImGui::BeginCombo("##ParticleColorMode", selected_particle_color_named))
	{
		for (uint32_t color_mode{ 0 }; color_mode < (uint32_t)ParticleColorMode::COLOR_MODE_COUNT; ++color_mode)
		{
			bool selected{ color_mode == (uint32_t)editor_->particle_color_mode_ };

			if (ImGui::Selectable(particle_color_mode_names[color_mode].c_str(), selected)) {
				editor_->SetParticleColorMode((ParticleColorMode)color_mode);
			}
		}
		ImGui::EndCombo();
	}

	if ((editor_->particle_color_mode_ != ParticleColorMode::FINAL_SHADING) && (editor_->particle_color_mode_ != ParticleColorMode::HIDDEN))
	{
		ImGui::SameLine();
		ImGui::Text("Max");
		ImGui::SameLine();
		ImGui::PushItemWidth(max_val_width);
		if (ImGui::DragFloat("##ParticleColorModeMaxValue", &editor_->particle_color_max_value_, 0.01f, 0.01f, 99.99f, "%.2f")) {
			editor_->UpdateParticleColorModeMaxValue();
		}
		ImGui::PopItemWidth();
	}

	ImGui::Text("Normals");
	ImGui::SameLine(SHADER_PROPERTY_ALIGNMENT);
	if (ImGui::Checkbox("##ShowRigidBodyNormals", &editor_->show_rigid_body_normals_)) {
		editor_->UpdateRigidBodyOverlayEnabled();
	}

	ImGui::End();
}

void EditorGui::Debug()
{
	if (!ImGui::Begin("Debug"))
	{
		ImGui::End();
		return;
	}

	std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
	float frame_milliseconds{ std::chrono::duration_cast<std::chrono::microseconds>(end_time - frame_start_time_).count() / 1000.0f };
	float second_timer{ std::chrono::duration_cast<std::chrono::microseconds>(end_time - second_start_time_).count() / 1000000.0f };

	if (second_timer >= 1.0f)
	{
		second_start_time_ = std::chrono::steady_clock::now();
		fps_ = frame_counter_ / second_timer;
		frame_counter_ = 0;
	}

	++frame_counter_;
	frame_start_time_ = std::chrono::steady_clock::now();

	ImGui::Text("%.3f ms", frame_milliseconds);
	ImGui::Text("%.1f fps", fps_);

	ImGui::End();
}

void EditorGui::UpdateViewportSize()
{
	const ImVec2 render_size{ ImGui::GetContentRegionAvail() };
	// If window closes ImGui sets its size to -1. So clamp to 0.
	uint32_t width{ (uint32_t)std::max(render_size.x, 0.0f) };
	uint32_t height{ (uint32_t)std::max(render_size.y, 0.0f) };
	renderer::Extent extent{ width, height };

	if (extent != viewport_extent_)
	{
		viewport_extent_ = extent;
		viewport_window_extent_ = ImGui::GetWindowSize();
		editor_->pumpkin_->SetEditorViewportSize(extent);
	}
}

void EditorGui::LoadProject()
{
	current_directory_ = popup_selected_file_ / ASSETS_RELATIVE_PATH;
	ProjectLoadGuiInfo info{ editor_->LoadProject(popup_selected_file_) };
	particle_count_ = info.particle_count;
	gen_shader_index_ = info.gen_shader_index;

	popup_selected_file_ = {};
	pumpkin_proj_selected_ = false;
	pumpkin_proj_loaded_ = true;
}

std::multiset<EditorNode*, EditorNodeCmp> EditorGui::GetSortedChildren(EditorNode* node)
{
	auto node_set{ node->node->GetChildren() };
	std::multiset<EditorNode*, EditorNodeCmp> sorted_editor_nodes{};

	for (pmk::Node* node : node->node->GetChildren()) {
		sorted_editor_nodes.insert(editor_->NodeToEditorNode(node));
	}

	return sorted_editor_nodes;
}
