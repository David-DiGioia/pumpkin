#include "gui.h"

#include <cmath>
#include <string>
#include <climits>
#include "imgui.h"
#include "implot/implot.h"
#include "curve_editor/curve_v122.hpp"

#include "editor.h"
#include "pumpkin.h"
#include "logger.h"
#include "music.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

constexpr uint32_t CURVE_EDITOR_POINTS{ 16 };

void EditorGui::Initialize(Editor* editor)
{
	editor_ = editor;
}

void EditorGui::InitializeGui()
{
	// Set ImGui settings.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = NULL; // Disable automatic saving of layout.
	io.IniSavingRate = 0.1f; // Small number so we can save often.
	io.WantCaptureKeyboard = true;
	ImGui::LoadIniSettingsFromDisk(default_layout_path.c_str());

	fundamental_wave_editor_data_.resize(CURVE_EDITOR_POINTS);
	fundamental_wave_editor_data_[0].x = -1;
	attack_editor_data_.resize(CURVE_EDITOR_POINTS);
	attack_editor_data_[0].x = -1;
	sustain_editor_data_.resize(CURVE_EDITOR_POINTS);
	sustain_editor_data_[0].x = -1;
	release_editor_data_.resize(CURVE_EDITOR_POINTS);
	release_editor_data_[0].x = -1;
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

		if (ImGui::Curve("Fundamental wave", ImVec2(content_region.x, 200), true, fundamental_wave_editor_data_.size(), fundamental_wave_editor_data_.data())) {
			status = "Curve has been changed.";
		}
		ImGui::Text(status.c_str());

		ImGui::Curve("Attack", ImVec2(content_region.x / 3.0f, 200), false, attack_editor_data_.size(), attack_editor_data_.data());
		ImGui::SameLine();
		ImGui::Curve("Sustain", ImVec2(content_region.x / 3.0f, 200), false, sustain_editor_data_.size(), sustain_editor_data_.data());
		ImGui::SameLine();
		ImGui::Curve("Release", ImVec2(content_region.x / 3.0f - 20, 200), false, release_editor_data_.size(), release_editor_data_.data());

		ImGui::DragFloat("Attack duration", &attack_duration_, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("Sustain duration", &sustain_duration_, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("Release duration", &release_duration_, 0.01f, 0.0f, 100.0f);
	}

	{
		const auto& audio_buffer{ editor_->instrument_.GetAudioBuffer() };

		std::vector<float> x_data(audio_buffer.size());
		std::vector<float> y_data(audio_buffer.size());

		for (uint16_t i{ 0 }; i < audio_buffer.size(); ++i)
		{
			x_data[i] = i / (float)pmk::SAMPLE_RATE;
			y_data[i] = (float)audio_buffer[i];
		}

		if (ImPlot::BeginPlot("Audio buffer plot"))
		{
			ImPlot::SetupAxisLimits(ImAxis_Y1, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());
			ImPlot::PlotLine("Audio buffer line", x_data.data(), y_data.data(), y_data.size());

			//float current_time{ editor_->instrument_.GetBufferTime() };
			//ImPlot::PlotVLines("Current time", &current_time, 1);

			ImPlot::EndPlot();
		}
	}

	if (ImGui::Button("Add wave"))
	{
		editor_->instrument_.AddWave({
			.fundamental_wave = [&](float x) { return ImGui::CurveValueSmoothAudio(x, fundamental_wave_editor_data_.size(), fundamental_wave_editor_data_.data()); },
			//.fundamental_wave = pmk::wave::Sin01,
			.harmonic_multipliers = [&](float time, uint32_t freq_multiple) { return harmonic_multiples_[freq_multiple - 1]; },
			.relative_frequency = 1.0f,
			});
	}

	ImGui::DragFloat("Amplitude", &amplitude_, 0.001f, 0.0f, 1.0f);
	editor_->instrument_.SetAmplitude(amplitude_);

	ImGui::Combo("Key", &current_key, pmk::note_names.data(), pmk::note_names.size());


	// Key input.
	std::vector<pmk::Note> notes{ GetNotesFromInput((pmk::Note)current_key, ScaleType::MAJOR) };

	editor_->instrument_.PlayNotes(notes);

	ImGui::SliderInt("Unison", &unison_, 1, 16);
	ImGui::SliderFloat("Unison radius", &unison_radius_, 0.0f, 30.0f);

	editor_->instrument_.SetUnison(unison_);
	editor_->instrument_.SetUnisonRadius(unison_radius_);

	for (uint32_t i{ 0 }; i < pmk::MAX_HARMONIC_MULTIPLE; ++i)
	{
		ImGui::PushID(i);
		ImGui::VSliderFloat("##harmonic", ImVec2(30, 160), &harmonic_multiples_[i], 0.0f, 1.0f);
		ImGui::SameLine();
		ImGui::PopID();
	}

	ImGui::End();
}

void EditorGui::UpdateViewportSize(const renderer::Extent& extent)
{
	if (extent != viewport_extent_)
	{
		viewport_extent_ = extent;
		editor_->pumpkin_->SetEditorViewportSize(extent);
	}
}
