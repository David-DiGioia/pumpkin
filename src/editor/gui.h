#pragma once

#include <cstdint>
#include "imgui.h"
#include "pumpkin.h"

class EditorGui
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

private:
	void MainMenu();

	void RightPane();

	void EngineViewport(ImTextureID* rendered_image_id);

	void UpdateViewportSize(const renderer::Extent& extent);

	pmk::Pumpkin* pumpkin_{};
	renderer::Extent viewport_extent_{};
};
