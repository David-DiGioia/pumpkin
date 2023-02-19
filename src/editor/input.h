#pragma once

#include "renderer_types.h"

class Editor;

constexpr float MINIMUM_MOVEMENT_SPEED{ 0.1f };

class EditorInput
{
public:
	void ProcessViewportInput(Editor* editor, const renderer::Extent& viewport_extent);

	void ProcessTreeViewInput(Editor* editor);

	void ProcessFileBrowserInput(Editor* editor);

private:
	glm::vec2 mouse_down_pos_{};
	bool should_cast_ray_on_release_{};
};