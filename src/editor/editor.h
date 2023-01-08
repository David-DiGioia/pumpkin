#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <string>
#include <filesystem>
#include "imgui.h"
#include "nlohmann/json.hpp"

#include "pumpkin.h"
#include "gui.h"
#include "camera_controller.h"

constexpr uint32_t NAME_BUFFER_SIZE{ 64 };

const std::filesystem::path ASSETS_RELATIVE_PATH{ "assets" };
const std::filesystem::path PROJECT_DATA_RELATIVE_PATH{ "pumpkin_project" };
const std::filesystem::path PROJECT_DATA_JSON_NAME{ "pumpkin_project.json" };
const std::filesystem::path VERTEX_DATA_FILE_NAME{ "vertex_data.bin" };
const std::filesystem::path INDEX_DATA_FILE_NAME{ "index_data.bin" };

enum class TransformType {
	NONE,
	TRANSLATE,
	ROTATE,
	SCALE,
};

enum class TransformLockFlags {
	NONE = 0x00u,
	X = 0x01u,
	Y = 0x02u,
	Z = 0x04u,
};

// Enable bitwise ORing transform flags together.
constexpr inline TransformLockFlags operator|(TransformLockFlags a, TransformLockFlags b)
{
	return static_cast<TransformLockFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// Enable bitwise ANDing transform flags together.
constexpr inline TransformLockFlags operator&(TransformLockFlags a, TransformLockFlags b)
{
	return static_cast<TransformLockFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

enum class TransformSpace {
	GLOBAL,
	LOCAL,
};

// Wrapper for renderer::Material that adds extra members only needed by the editor.
class EditorMaterial
{
public:
	EditorMaterial(renderer::Material* pmk_material, const std::string& name);

	EditorMaterial(renderer::Material* pmk_material);

	~EditorMaterial();

	std::string GetName() const;

	char* GetNameBuffer() const;

	renderer::Material* material{};
	uint32_t user_count{}; // Number of meshes that use this material.
private:
	char* name_buffer_{};
};

// Wrapper for pmk::Node that adds extra members only needed by the editor.
class EditorNode
{
public:
	EditorNode(pmk::Node* pmk_node, const std::string& name);

	EditorNode(pmk::Node* pmk_node);

	~EditorNode();

	std::string GetName() const;

	char* GetNameBuffer() const;

	pmk::Node* node;
private:
	char* name_buffer_;
};

struct Transform
{
	glm::vec3 position{};
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
	glm::quat rotation{};
};

struct TransformInfo
{
	TransformType type{ TransformType::NONE };
	TransformLockFlags lock{ TransformLockFlags::NONE };
	TransformSpace space{ TransformSpace::GLOBAL };
	glm::vec2 mouse_start_pos{ -1.0f, -1.0f };
	glm::vec3 average_start_pos{};

	// Save a copy of the selected nodes' original transforms, since we transform relative to them each frame, and may want to restore them.
	std::unordered_map <EditorNode*, Transform> original_transforms{};
};

class Editor
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void CleanUp();

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	renderer::ImGuiCallbacks GetEditorInfo();

	CameraController& GetCameraController();

	pmk::Pumpkin* GetPumpkin();

	EditorNode* GetRootNode() const;

	std::unordered_map<uint32_t, EditorNode*>& GetNodeMap();

	void SetNodeSelection(EditorNode* node, bool selected);

	void ToggleNodeSelection(EditorNode* node);

	void SelectNode(EditorNode* node);

	void DeselectNode(EditorNode* node);

	bool IsNodeSelected(EditorNode* node);

	bool SelectionEmpty() const;

	// Let editor decided what happens depending on if multiselect is enabled.
	void NodeClicked(EditorNode* node);

	void FileClicked(const std::filesystem::path& path);

	void FileDoubleClicked(const std::filesystem::path& path);

	bool IsFileSelected(const std::filesystem::path& path);

	void SetFileSelection(const std::filesystem::path& path, bool selected);

	void ToggleFileSelection(const std::filesystem::path& path);

	void SelectFile(const std::filesystem::path& path);

	void DeselectFile(const std::filesystem::path& path);

	std::filesystem::path GetProjectDirectory() const;

	void SaveProject() const;

	void NewProject(const std::filesystem::path& proj_dir);

	void LoadProject(const std::filesystem::path& proj_dir);

	void LoadNodeData(const nlohmann::json& j);

	// Get the EditorNode which contains the specified pmk::Node.
	EditorNode* NodeToEditorNode(pmk::Node* node);

	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	//
	// path: The path relative to the assets folder.
	void ImportGLTF(const std::filesystem::path& path);

	void SetMultiselect(bool multiselect);

	EditorNode* GetActiveSelectionNode();

	// If a node is actively being transformed via hotkeys, this will tell you the type of transformation it's undergoing.
	TransformType GetActiveTransformType() const;

	void SetActiveTransformType(TransformType state);

	void SetTransformLock(TransformLockFlags lock);

	// Process the transform input while the transform state is not TransformState::NONE. This gives live update from user input.
	// 
	// mouse_pos: The mouse position using units of viewport height.
	void ProcessTransformInput(const glm::vec2& mouse_pos);

	void ApplyTransformInput();

	void CancelTransformInput();

	const EditorGui& GetGui() const;

	glm::vec3 GetSelectedNodesAveragePosition() const;

private:
	void ProcessTranslationInput(const glm::vec2& mouse_delta);

	void ProcessRotationInput(const glm::vec2& mouse_pos);

	void ProcessScaleInput(const glm::vec2& mouse_pos);

	// Temporarily save original transforms of selected objects before we modify them.
	void CacheOriginalTransforms();

	// Query if any ancestor of a node is selected (parent, parent's parent, etc).
	bool IsNodeAncestorSelected(EditorNode* node);

	// Get the viewport position, with units of viewport height, where (0, 0) is the top left of the viewport.
	glm::vec2 WorldToScreenSpace(const glm::vec3& world_pos) const;

	// Get all the material indices in the order of the geometries associated with a node, if it has a mesh.
	const std::vector<int>& GetMaterialIndicesFromNode(EditorNode* node);

	void SetNodeMaterial(EditorNode* node, uint32_t geometry_index, int material_index);

	// Makes material into a single-user copy. Returns index to the newly created material.
	uint32_t MakeMaterialUnique(int material_index);

	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};
	CameraController controller_{};

	EditorNode* root_node_{};                              // All other nodes are a descendent of the root node.
	EditorNode* active_selection_node_{ nullptr };         // There can be multiple selected nodes but only one actively selected node.
	std::unordered_set<EditorNode*> selected_nodes_{};     // Set of all selected nodes. Having a set makes it possible to ierate over only selected nodes.
	std::unordered_map<uint32_t, EditorNode*> node_map_{}; // The key of this map is pmk::Node::node_id. This allows finding an EditorNode from a pmk::Node.
	std::vector<EditorMaterial*> materials_{};             // List of EditorMaterials in same order as the renderer's material list, so material index is valid here too.

	std::filesystem::path project_directory_{};                  // The root directory of the user's project.
	std::filesystem::path active_selection_file_{};              // The actively selected file.
	std::unordered_set<std::filesystem::path> selected_files_{}; // Indices of all selected files in the file browser.

	bool multi_select_enabled_{ false }; // Selecting a selectable does not deselect all others when enabled.

	TransformInfo transform_info_{};
};
