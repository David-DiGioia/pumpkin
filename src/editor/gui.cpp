#include "gui.h"

#include <cmath>
#include <string>
#include <climits>
#include "imgui.h"
#include "implot/implot.h"
#include "curve_editor/curve_v122.hpp"
#include "imsequencer/imsequencer.h"

#include "editor.h"
#include "pumpkin.h"
#include "logger.h"
#include "input.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

constexpr uint32_t CURVE_EDITOR_POINTS{ 16 };

void EditorGui::Initialize(Editor* editor)
{
	editor_ = editor;
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

	MainMenu();
	TreeView();
	RightPane();
	ImGui::ShowDemoWindow();
	EngineViewport(rendered_image_id);
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

void EditorGui::MainMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			MainMenuSaveDefaultLayout();
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

	bool node_open{ ImGui::TreeNodeEx((void*)(intptr_t)root->node->node_id, node_flags, "Node %d", root->node->node_id) };

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

	if (clicked_node) {
		editor_->ToggleNodeSelection(clicked_node);
	}

	ImGui::End();
}

void EditorGui::RightPane()
{
	if (!ImGui::Begin("Right pane"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGui::Text("Welcome to the Pumpkin engine!");

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

	ImVec2 render_size{ ImGui::GetContentRegionAvail() };
	// If window closes ImGui sets its size to -1. So clamp to 0.
	uint32_t width{ (uint32_t)std::max(render_size.x, 0.0f) };
	uint32_t height{ (uint32_t)std::max(render_size.y, 0.0f) };
	UpdateViewportSize({ width, height });

	if (!success)
	{
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused()) {
		ProcessViewportInput(editor_);
	}

	if ((viewport_extent_.width != 0) && (viewport_extent_.height != 0)) {
		ImGui::Image(*rendered_image_id, render_size);
	}

	ImGui::End();
}

void EditorGui::UpdateViewportSize(const renderer::Extent& extent)
{
	if (extent != viewport_extent_)
	{
		viewport_extent_ = extent;
		editor_->pumpkin_->SetEditorViewportSize(extent);
	}
}
