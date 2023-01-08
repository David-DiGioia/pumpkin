#include "gui.h"

#include <cmath>
#include <string>
#include <climits>
#include "imgui.h"
#include "glm/gtc/type_ptr.hpp"

#include "editor.h"
#include "pumpkin.h"
#include "logger.h"
#include "input.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

constexpr uint32_t PROJECT_NAME_BUFFER_SIZE{ 16 };

void EditorGui::Initialize(Editor* editor)
{
	editor_ = editor;
	popup_name_buffer_ = new char[PROJECT_NAME_BUFFER_SIZE] {};
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
	ImGui::LoadIniSettingsFromDisk(default_layout_path.c_str());
}

void EditorGui::DrawGui(ImTextureID* rendered_image_id)
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	CheckProjectSelectionPopup();
	MainMenu();
	TreeView();
	ImGui::ShowDemoWindow();
	NodeProperties();
	EngineViewport(rendered_image_id);
	FileBrowser();
	CameraControls();
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

void MainMenuSaveDefaultLayout()
{
	if (ImGui::MenuItem("Save as default layout"))
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantSaveIniSettings)
		{
			ImGui::SaveIniSettingsToDisk(default_layout_path.c_str());
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
		for (pmk::Node* node : root->node->GetChildren()) {
			DrawTreeNode(editor_->NodeToEditorNode(node), out_node_clicked);
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
	for (pmk::Node* node : root_node->node->GetChildren()) {
		DrawTreeNode(editor_->NodeToEditorNode(node), &clicked_node);
	}

	// If the defocused window is clicked in, it seems the IsWindowFocused is delayed a frame before it returns true.
	if (ImGui::IsWindowFocused() || clicked_node) {
		ProcessTreeViewInput(editor_);
	}

	if (clicked_node) {
		editor_->NodeClicked(clicked_node);
	}

	ImGui::End();
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
		ImGui::InputText("Node name", active_node->GetNameBuffer(), NAME_BUFFER_SIZE);
		ImGui::DragFloat3("Position", glm::value_ptr(active_node->node->position), 0.1f);
		ImGui::DragFloat3("Scale", glm::value_ptr(active_node->node->scale), 0.1f);
		auto& rot{ active_node->node->rotation };
		ImGui::Text("%.2f  %.2f  %.2f  %.2f Rotation", rot.x, rot.y, rot.z, rot.w);

		std::vector<int> node_materials{ editor_->GetMaterialIndicesFromNode(active_node) };
		std::vector<const char*> node_materials_strings(node_materials.size());
		std::transform(node_materials.begin(), node_materials.end(), node_materials_strings.begin(),
			[=](int mat_idx) { return editor_->materials_[mat_idx]->GetNameBuffer(); });

		ImGui::Dummy(ImVec2{ 0.0f, 20.0f }); // Spacing.
		ImGui::Text("Material");
		ImGui::ListBox("##MaterialList", &material_selected_geometry_index_, node_materials_strings.data(), node_materials_strings.size(), 4);

		if (material_selected_geometry_index_ >= 0 && material_selected_geometry_index_ < (int)node_materials_strings.size())
		{
			EditorMaterial* mat{ editor_->materials_[node_materials[material_selected_geometry_index_]] };

			// Combo box to swap selected material for a different existing material.
			material_selected_combo_ = node_materials[material_selected_geometry_index_];
			if (ImGui::BeginCombo("##MaterialCombo", nullptr, ImGuiComboFlags_NoPreview))
			{
				for (uint32_t mat_idx = 0; mat_idx < (uint32_t)editor_->materials_.size(); ++mat_idx)
				{
					const bool is_selected{ material_selected_combo_ == mat_idx };
					if (ImGui::Selectable(editor_->materials_[mat_idx]->GetNameBuffer(), is_selected)) {
						editor_->SetNodeMaterial(active_node, material_selected_geometry_index_, mat_idx);
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus).
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::InputText("##MaterialName", mat->GetNameBuffer(), NAME_BUFFER_SIZE);

			if (mat->user_count > 1)
			{
				ImGui::SameLine();
				std::string user_count_string{ std::to_string(mat->user_count) };
				if (ImGui::Button(user_count_string.c_str()))
				{
					uint32_t mat_copy{ editor_->MakeMaterialUnique((uint32_t)material_selected_combo_) };
					editor_->SetNodeMaterial(active_node, material_selected_geometry_index_, mat_copy);
				}
			}

			bool mat_changed{ false };
			mat_changed |= ImGui::ColorEdit3("Color", glm::value_ptr(mat->material->color));
			mat_changed |= ImGui::DragFloat("Metallic", &mat->material->metallic, 0.01f, 0.000f, 1.0f);
			mat_changed |= ImGui::DragFloat("Roughness", &mat->material->roughness, 0.01f, 0.001f, 0.999f);
			mat_changed |= ImGui::DragFloat("IOR", &mat->material->ior, 0.01f, 1.0f, 2.0f);
			mat_changed |= ImGui::DragFloat("Emission", &mat->material->emission, 0.01f, 0.0f, 1000.0f);

			if (mat_changed) {
				editor_->pumpkin_->UpdateMaterials();
			}
		}
	}
	else {
		ImGui::Text("No actively selected node.");
	}

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

	if (ImGui::IsWindowFocused()) {
		ProcessViewportInput(editor_);
	}

	if ((viewport_extent_.width != 0) && (viewport_extent_.height != 0)) {
		ImGui::Image(*rendered_image_id, ImGui::GetContentRegionAvail());
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
		auto project_dir{ popup_current_directory_ / popup_name_buffer_ };
		editor_->NewProject(project_dir);
		current_directory_ = project_dir / ASSETS_RELATIVE_PATH;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();

	ImGui::Text("\nLoad existing project");
	ImGui::PushID(1);
	if (ImGui::Button("Parent directory")) {
		popup_current_directory_ = popup_current_directory_.parent_path();
	}

	ImGui::SameLine();
	ImGui::Text(popup_current_directory_.string().c_str());
	ImGui::PopID();

	ImGui::BeginChild("Project selection child", ImVec2(ImGui::GetContentRegionAvail().x, 200), false, 0);

	if (fs::exists(popup_current_directory_) && fs::is_directory(popup_current_directory_))
	{
		int idx{ 0 };

		for (const fs::directory_entry& entry : fs::directory_iterator(popup_current_directory_))
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
					popup_current_directory_ = entry;
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
				ProcessFileBrowserInput(editor_);
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

	ImGui::SliderFloat("Speed", &editor_->GetCameraController().MovementSpeed(), MINIMUM_MOVEMENT_SPEED, 10000.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("FOV", &editor_->GetCameraController().GetCamera()->fov, 30.0f, 90.0f, "%.1f", ImGuiSliderFlags_None);

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
	editor_->LoadProject(popup_selected_file_);
	popup_selected_file_ = {};
	pumpkin_proj_selected_ = false;
}
