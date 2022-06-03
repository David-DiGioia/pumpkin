#pragma once

#include <vector>
#include "imgui.h"

#include "pumpkin.h"
#include "gui.h"

class Editor
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	renderer::ImGuiCallbacks GetEditorInfo();

private:
	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};
};
