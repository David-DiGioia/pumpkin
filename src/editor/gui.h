#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <unordered_set>
#include <set>
#include <filesystem>
#include "pumpkin.h"
#include "imgui.h"
#include "input.h"

class Editor;
class EditorNode;

// Comparator to compare EditorNode pointers by name.
struct EditorNodeCmp
{
	bool operator()(EditorNode* a, EditorNode* b) const;
};

enum class ParticleColorMode
{
	NONE,
	MASS,
	MU,
	LAMBDA,
	VELOCITY,
	COMPRESSION,
	TENSION,

	COLOR_MODE_COUNT,
};

const std::array<std::string, (uint32_t)ParticleColorMode::COLOR_MODE_COUNT> particle_color_mode_names{
	"None",
	"Mass",
	"Mu",
	"Lambda",
	"Velocity",
	"Compression",
	"Tension",
};

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

	void MainMenuSaveDefaultLayout();

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

	void ParticleEditor();

	void Debug();

	void UpdateViewportSize();

	void LoadProject();

	// Returns true if texture property is being used, or false if slider should be displayed.
	bool MaterialTextureProperty(const std::string& name, bool* show_tex_ui, uint32_t* texture_index, bool* mat_changed, bool color_data);

	void ShaderProperty(const std::string& name, uint32_t* shader_index, bool* compile_error);

	std::multiset<EditorNode*, EditorNodeCmp> GetSortedChildren(EditorNode* node);

	Editor* editor_{};
	EditorInput input_{};
	renderer::Extent viewport_extent_{};        // Dimension of the 3D viewport.
	renderer::Extent viewport_window_extent_{}; // Dimension of the window containing 3D viewport, including header etc.
	std::filesystem::path current_directory_{};

	int material_selected_geometry_index_{};

	std::chrono::steady_clock::time_point frame_start_time_{};  // For timing frame duration.
	std::chrono::steady_clock::time_point second_start_time_{}; // For calculating FPS.
	uint32_t frame_counter_{};                                  // For calculating FPS.
	float fps_{};

	std::filesystem::path project_popup_current_directory_{ "C:\\" };
	std::filesystem::path texture_popup_current_directory_{};
	std::filesystem::path shader_popup_current_directory_{};
	std::filesystem::directory_entry popup_selected_file_{};
	bool open_project_selection_popup_{ true };
	char* popup_name_buffer_;
	bool pumpkin_proj_selected_{ false };
	bool pumpkin_proj_loaded_{ false };

	uint32_t gen_shader_index_{ renderer::NULL_INDEX };
	bool gen_shader_compile_error_{ false };
};
