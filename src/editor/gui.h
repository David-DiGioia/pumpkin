#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "pumpkin.h"
#include "imgui.h"

class Editor;
class EditorNode;

class EditorGui
{
public:
	void Initialize(Editor* editor);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

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

	void UpdateViewportSize(const renderer::Extent& extent);

	Editor* editor_{};
	renderer::Extent viewport_extent_{};
};
