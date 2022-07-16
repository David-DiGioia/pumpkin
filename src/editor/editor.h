#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <string>
#include "imgui.h"

#include "pumpkin.h"
#include "gui.h"
#include "camera_controller.h"

constexpr uint32_t NODE_NAME_BUFFER_SIZE{ 64 };

enum class TransformType {
	NONE,
	TRANSLATE,
	ROTATE,
	SCALE,
};

enum class TransformLock {
	NONE,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
};

enum class TransformSpace {
	GLOBAL,
	LOCAL,
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
	TransformLock lock{ TransformLock::NONE };
	TransformSpace space{ TransformSpace::GLOBAL };
	glm::vec2 mouse_start_pos{ -1.0f, -1.0f };

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

	// Get the EditorNode which contains the specified pmk::Node.
	EditorNode* NodeToEditorNode(pmk::Node* node);

	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	//
	// path: The path relative to the assets folder.
	void ImportGLTF(const std::string& path);

	void SetMultiselect(bool multiselect);

	EditorNode* GetActiveSelectionNode();

	// If a node is actively being transformed via hotkeys, this will tell you the type of transformation it's undergoing.
	TransformType GetActiveTransformType() const;

	void SetActiveTransformType(TransformType state);

	// Process the transform input while the transform state is not TransformState::NONE. This gives live update from user input.
	// 
	// mouse_pos: The mouse position using units of viewport height.
	void ProcessTransformInput(const glm::vec2& mouse_pos);

	void ApplyTransformInput();

	void CancelTransformInput();

private:
	// Temporarily save original transforms of selected objects before we modify them.
	void CacheOriginalTransforms();

	glm::vec3 GetSelectedNodesAveragePosition() const;

	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};
	CameraController controller_{};

	EditorNode* root_node_{};                              // All other nodes are a descendent of the root node.
	EditorNode* active_selection_node_{ nullptr };         // There can be multiple selected nodes but only one actively selected node.
	std::unordered_set<EditorNode*> selected_nodes_{};     // Set of all selected nodes. Having a set makes it possible to ierate over only selected nodes.
	std::unordered_map<uint32_t, EditorNode*> node_map_{}; // The key of this map is pmk::Node::node_id.

	bool multi_select_enabled_{ false }; // Selecting a node does not deselect all other nodes when enabled.

	TransformInfo transform_info_{};
};
