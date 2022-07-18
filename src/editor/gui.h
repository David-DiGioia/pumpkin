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

	void UpdateViewportSize(const renderer::Extent& extent);

	Editor* editor_{};
	renderer::Extent viewport_extent_{};        // Dimension of the 3D viewport.
	renderer::Extent viewport_window_extent_{}; // Dimension of the window containing 3D viewport, including header etc.
};
