#include "editor.h"

#include <string>
#include "imgui.h"

#include "gui.h"

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
	gui_.Initialize(this);

	// Set editor root node to match Pumpkin's scene root node.
	node_map_[scene.GetRootNode()->node_id] = new EditorNode{ scene.GetRootNode() };
	root_node_ = node_map_[scene.GetRootNode()->node_id];
}

void Editor::CleanUp()
{
	for (auto& pair : node_map_) {
		delete pair.second;
	}
	node_map_.clear();
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

void Editor::NodeClicked(EditorNode* node)
{
	if (multi_select_enabled_) {
		ToggleNodeSelection(node);
	}
	else {
		SelectNode(node);
	}
}

EditorNode* Editor::NodeToEditorNode(pmk::Node* node)
{
	return node_map_[node->node_id];
}

void Editor::ImportGLTF(const std::string& path)
{
	std::string prefix{ "../../../assets/" };
	auto& nodes{ pumpkin_->GetScene().GetNodes() };

	// The starting index before we add more nodes.
	uint32_t i{ (uint32_t)nodes.size() };

	// Add new nodes to scene.
	pumpkin_->GetScene().ImportGLTF(prefix + path);

	// Make a wrapper EditorNode for each imported pmk::Node.
	while (i < nodes.size())
	{
		node_map_[nodes[i]->node_id] = new EditorNode{ nodes[i] };
		++i;
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

EditorNode::EditorNode(pmk::Node* pmk_node, const std::string& name)
	: node{ pmk_node }
	, name_buffer_{ new char[NODE_NAME_BUFFER_SIZE] {} }
{
	std::strcpy(name_buffer_, name.c_str());
}

EditorNode::EditorNode(pmk::Node* pmk_node)
	: EditorNode(pmk_node, std::string{ "Node " } + std::to_string(pmk_node->node_id) )
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
