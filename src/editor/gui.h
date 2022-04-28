#pragma once

#include "imgui.h"

class EditorGui
{
public:
	void Initialize();

	void DrawGui(ImTextureID rendered_image_id);

private:
	void MainMenu();

	void RightPane();

	void EngineViewport();
};
