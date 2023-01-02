#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <unordered_set>
#include <filesystem>
#include "pumpkin.h"
#include "imgui.h"

class Editor;
class EditorNode;

class EditorGui
{
public:
	// Initialize state of EditorGui, but nothing ImGui related. Called after Editor is initialized.
	void Initialize(Editor* editor);

	void CleanUp();

	// Initialize everything ImGui related. Called when Pumpkin is initialized, which is before Editor is initialized.
	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	void CheckProjectSelectionPopup();

	// Get the extent of the 3D viewport render area.
	const renderer::Extent& GetViewportExtent() const;

	// Get the extent of the window containing the 3D viewport.
	const renderer::Extent& GetViewportWindowExtent() const;

private:
	void MainMenu();

	// Recursive function for drawing tree view.
	//
	// root:             The node to draw, along with its subtree.
	// out_node_clicked: Pointer to whichever node was clicked this frame, or nullptr.
	void DrawTreeNode(EditorNode* root, EditorNode** out_node_clicked);

	void TreeView();

	void NodeProperties();

	void EngineViewport(ImTextureID* rendered_image_id);

	void FileBrowser();

	void ProjectSelectionPopup();

	void CameraControls();

	void UpdateViewportSize();

	void LoadProject();

	Editor* editor_{};
	renderer::Extent viewport_extent_{};        // Dimension of the 3D viewport.
	renderer::Extent viewport_window_extent_{}; // Dimension of the window containing 3D viewport, including header etc.
	std::filesystem::path current_directory_{};

	int material_selected_geometry_index_{};
	int material_selected_combo_{};

	std::filesystem::path popup_current_directory_{ "D:\\dev\\pumpkin_projects" };
	std::filesystem::directory_entry popup_selected_file_{};
	bool open_project_selection_popup_{ true };
	char* popup_name_buffer_;
	bool pumpkin_proj_selected_{ false };
};
