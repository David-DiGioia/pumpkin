#pragma once

#include "imgui.h"

#include "pumpkin.h"
#include "editor_backend.h"
#include "gui.h"

class Editor
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	renderer::EditorInfo GetEditorInfo();


private:
	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};

	pmk::Instrument instrument_{};
};
