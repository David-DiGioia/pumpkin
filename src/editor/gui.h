#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "pumpkin.h"
#include "imgui.h"

class Editor;

class EditorGui
{
public:
	void Initialize(Editor* editor);

	void InitializeGui();

	void DrawGui(ImTextureID* rendered_image_id);

private:
	void MainMenu();

	void RightPane();

	void EngineViewport(ImTextureID* rendered_image_id);

	void AudioWindow();

	void UpdateViewportSize(const renderer::Extent& extent);

	Editor* editor_{};
	renderer::Extent viewport_extent_{};

	std::vector<ImVec2> fundamental_wave_editor_data_{};
	float frequency_{ 100.0f };
	std::array<float, pmk::MAX_HARMONIC_MULTIPLE> harmonic_multiples_{ 1.0f };
};
