#pragma once

#include <cstdint>
#include <vector>
#include "imgui.h"
#include "pumpkin.h"
#include "SFML/Audio.hpp"

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

	void AudioWindow();

	void UpdateViewportSize(const renderer::Extent& extent);

	void PlayTestSound();

	pmk::Pumpkin* pumpkin_{};
	renderer::Extent viewport_extent_{};

	std::vector<float> x_data_{};

	std::vector<ImVec2> curve_editor_data_{};

	sf::SoundBuffer sound_buffer_{};
	sf::Sound sound_{};
};
