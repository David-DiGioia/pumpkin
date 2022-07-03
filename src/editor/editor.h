#pragma once

#include <vector>
#include <unordered_map>
#include <limits>
#include "imgui.h"

#include "pumpkin.h"
#include "gui.h"
#include "camera_controller.h"

constexpr uint32_t NULL_NODE_ID{ std::numeric_limits<uint32_t>::max() };

struct EditorNode
{
	pmk::Node* node;
	bool selected;
};

class Editor
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	renderer::ImGuiCallbacks GetEditorInfo();

	CameraController& GetCameraController();

	pmk::Pumpkin* GetPumpkin();

	// Import the whole GLTF hierarchy, adding all nodes to scene.
	// Note that Blender doesn't export cameras or lights.
	//
	// path: The path relative to the assets folder.
	void ImportGLTF(const std::string& path);

private:
	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};
	CameraController controller_{};

	EditorNode* root_node_{};                             // All other nodes are a descendent of the root node.
	std::unordered_map<uint32_t, EditorNode> node_map_{}; // The key of this map is pmk::Node::node_id.
	uint32_t active_selection_node_id_{ NULL_NODE_ID };   // There can be multiple selected nodes but only one actively selected node.
};
