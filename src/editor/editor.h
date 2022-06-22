#pragma once

#include <vector>
#include "imgui.h"

#include "pumpkin.h"
#include "gui.h"
#include "camera_controller.h"

class Editor
{
public:
	void Initialize(pmk::Pumpkin* pumpkin);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

	renderer::ImGuiCallbacks GetEditorInfo();

	CameraController& GetCameraController();

	pmk::Pumpkin* GetPumpkin();

private:
	friend class EditorGui;

	pmk::Pumpkin* pumpkin_{};
	EditorGui gui_{};
	CameraController controller_{};
};
