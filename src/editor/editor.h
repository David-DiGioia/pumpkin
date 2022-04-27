#pragma once

#include "imgui.h"

#include "pumpkin.h"
#include "editor_backend.h"

class Editor
{
public:
	void Initialize();

	void SetRenderedImageID(ImTextureID rendered_image);

	renderer::EditorInfo GetEditorInfo();

private:
	ImTextureID rendered_image_id_{}; // Renderer draws to this image.
};
