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

	std::vector<ImVec2> fundamental_wave_editor_data_{ {0.0f, 0.5f}, {0.25f, 0.0f},  {0.75f, 1.0f}, {1.0f, 0.5f}, {-1.0f, 0.0f} };
	std::vector<ImVec2> attack_editor_data_{ {0.0f, 0.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f} };
	std::vector<ImVec2> sustain_editor_data_{ {0.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f} };
	std::vector<ImVec2> release_editor_data_{ {0.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f} };
	float attack_duration_{ 0.1f };
	float sustain_duration_{ 0.1f };
	float release_duration_{ 0.1f };

	std::array<float, pmk::MAX_HARMONIC_MULTIPLE> harmonic_multiples_{ 1.0f };
	int current_key_{ (int)pmk::Note::C_3 };
	int current_instrument_index_{};
	int unison_{ 1 };
	float unison_radius_{ 0.1f };
	float amplitude_{ 0.1f };
};
