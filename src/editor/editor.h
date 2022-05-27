#pragma once

#include <vector>
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

	void AddInstrument(const char* name);

	void SetActiveInstrument(int index);

private:
	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};

	pmk::Instrument* active_instrument_{};
	std::vector<pmk::Instrument> instruments_{};
	std::vector<const char*> instrument_names_{};
};
