#include "gui.h"

#include <cmath>
#include <string>
#include "imgui.h"
#include "implot/implot.h"
#include "curve_editor/curve_v122.hpp"
#include "SFML/Audio.hpp"

#include "logger.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

constexpr uint32_t PLOT_SIZE{ 20 };

void EditorGui::Initialize(pmk::Pumpkin* pumpkin)
{
	pumpkin_ = pumpkin;
}

void EditorGui::InitializeGui()
{
	// Set ImGui settings.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = NULL; // Disable automatic saving of layout.
	io.IniSavingRate = 0.1f; // Small number so we can save often.
	ImGui::LoadIniSettingsFromDisk(default_layout_path.c_str());

	x_data_.resize(PLOT_SIZE);
	for (uint32_t i{ 0 }; i < PLOT_SIZE; ++i)
	{
		x_data_[i] = i;
	}

	curve_editor_data_.resize(10);
	curve_editor_data_[0].x = -1;
}

void EditorGui::DrawGui(ImTextureID* rendered_image_id)
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	MainMenu();
	RightPane();
	AudioWindow();
	EngineViewport(rendered_image_id);

	ImGui::ShowDemoWindow();
}

void MainMenuSaveDefaultLayout()
{
	if (ImGui::MenuItem("Save as default layout"))
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantSaveIniSettings)
		{
			ImGui::SaveIniSettingsToDisk(default_layout_path.c_str());
			logger::Print("Saved ImGui layout to disk.\n");
			io.WantSaveIniSettings = false;
		}
	}
}

void EditorGui::MainMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			MainMenuSaveDefaultLayout();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void EditorGui::RightPane()
{
	if (!ImGui::Begin("Right pane"))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGui::Text("Welcome to the Pumpkin engine!");

	ImGui::End();
}

// The 3D scene rendered from Renderer.
void EditorGui::EngineViewport(ImTextureID* rendered_image_id)
{
	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	bool success{ ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar) };
	ImGui::PopStyleVar(3);

	ImVec2 render_size{ ImGui::GetContentRegionAvail() };
	// If window closes ImGui sets its size to -1. So clamp to 0.
	uint32_t width{ (uint32_t)std::max(render_size.x, 0.0f) };
	uint32_t height{ (uint32_t)std::max(render_size.y, 0.0f) };
	UpdateViewportSize({ width, height });

	if (!success)
	{
		ImGui::End();
		return;
	}

	if ((viewport_extent_.width != 0) && (viewport_extent_.height != 0)) {
		ImGui::Image(*rendered_image_id, render_size);
	}

	ImGui::End();
}

constexpr float TWO_PI{ 6.28318530718f };
constexpr uint32_t SAMPLE_RATE{ 44100 };

template <typename Wave>
int16_t SampleWave(uint32_t tick, float frequency, float amplitude, Wave wave)
{
	// Sampling frequency is the number of samples (ticks) per second, so dividing
	// by number of cycles in a second (freq) gives us the number of ticks in a cycle.
	float ticks_per_cycle{ SAMPLE_RATE / frequency };

	// Ratio of where we sample from one cycle of the sine wave. In the range [0, 1].
	float cycle_ratio{ tick / ticks_per_cycle };

	// Convert the range [0, 1] to [0, 2pi].
	//float radians{ TWO_PI * cycle_ratio };

	// Amplitude is in the range [0, 1] so we convert the amplitude to the full range
	// of a 16 bit signed integer.
	int16_t amplitude_discrete{ (int16_t)(std::numeric_limits<int16_t>::max() * amplitude) };

	// Convert the the wave sample from [-1, 1] to the full range of int16_t.
	return (int16_t)(amplitude_discrete * wave(cycle_ratio));
}

// Get a single audio sample from a sinewave.
//
// tick - The index of the sample. Every 44100 samples is one second of audio.
// frequency - The frequency of the wave that's being sampled from.
// amplitude - The volume of the sample. Should be in the range [0, 1].
int16_t SineWave(uint32_t tick, float frequency, float amplitude)
{
	return SampleWave(tick, frequency, amplitude, std::sinf);
}

int16_t CustomWave(uint32_t tick, float frequency, float amplitude, const std::vector<ImVec2>& curve_data)
{
	return SampleWave(tick, frequency, amplitude, [&](float x) {
		float integer_part{};
		return ImGui::CurveValueSmoothAudio(std::modff(x, &integer_part), curve_data.size(), curve_data.data());
	});
}

void EditorGui::PlayTestSound()
{
	float seconds{ 2.0f };

	std::vector<sf::Int16> samples(seconds * SAMPLE_RATE);

	for (uint32_t i{ 0 }; i < samples.size(); ++i)
	{
		float freq{ 200.0f };
		samples[i] = CustomWave(i, freq, 0.4f, curve_editor_data_);
	}

	sound_buffer_.loadFromSamples(samples.data(), samples.size(), 1, SAMPLE_RATE);

	sound_.setBuffer(sound_buffer_);
	sound_.play();
}

void EditorGui::AudioWindow()
{
	bool success{ ImGui::Begin("Audio") };
	if (!success)
	{
		ImGui::End();
		return;
	}

	{
		ImVec2 content_region{ ImGui::GetContentRegionAvail() };

		std::string status{ "Curve has not been changed." };

		if (ImGui::Curve("Das editor", ImVec2(content_region.x, 200), curve_editor_data_.size(), curve_editor_data_.data()))
		{
			status = "Curve has been changed.";
		}

		float value_you_care_about = ImGui::CurveValue(0.7f, curve_editor_data_.size(), curve_editor_data_.data());

		ImGui::Text("Curve value at 0.7f: %.2f", value_you_care_about);

		ImGui::Text(status.c_str());
	}

	{
		constexpr uint32_t plot_size{ 1000 };
		std::vector<float> x_data(plot_size);
		std::vector<float> y_data(plot_size);

		for (uint32_t i{ 0 }; i < plot_size; ++i)
		{
			float integer_part{};

			float x{ i / 100.0f };
			x_data[i] = x;
			y_data[i] = ImGui::CurveValueSmoothAudio(std::modff(x, &integer_part), curve_editor_data_.size(), curve_editor_data_.data());
		}

		if (ImPlot::BeginPlot("My Plot"))
		{
			ImPlot::PlotLine("My Line Plot", x_data.data(), y_data.data(), plot_size);
			ImPlot::EndPlot();
		}
	}

	if (ImGui::Button("Play audio")) {
		PlayTestSound();
	}

	ImGui::End();
}

void EditorGui::UpdateViewportSize(const renderer::Extent& extent)
{
	if (extent != viewport_extent_)
	{
		viewport_extent_ = extent;
		pumpkin_->SetEditorViewportSize(extent);
	}
}
