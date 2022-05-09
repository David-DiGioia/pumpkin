#include "gui.h"

#include <cmath>
#include <string>
#include "imgui.h"
#include "implot/implot.h"
#include "curve_editor/curve_v122.hpp"
#include "editor.h"
#include "pumpkin.h"

#include "logger.h"

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
	ImGui::LoadIniSettingsFromDisk(default_layout_path.c_str());

	fundamental_wave_editor_data_.resize(CURVE_EDITOR_POINTS);
	fundamental_wave_editor_data_[0].x = -1;

	harmonics_editor_data_.resize(CURVE_EDITOR_POINTS);
	harmonics_editor_data_[0].x = -1;
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

		if (ImGui::Curve("Fundamental wave", ImVec2(content_region.x, 200), fundamental_wave_editor_data_.size(), fundamental_wave_editor_data_.data())) {
			status = "Curve has been changed.";
		}
		ImGui::Text(status.c_str());

		if (ImGui::Curve("Harmonics", ImVec2(content_region.x, 200), harmonics_editor_data_.size(), harmonics_editor_data_.data())) {
		}
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
			ImPlot::SetupAxisLimits(ImAxis_Y1, -3500.0, 3500.0);
			ImPlot::PlotLine("Audio buffer line", x_data.data(), y_data.data(), y_data.size());
			
			float current_time{ editor_->instrument_.GetTime() };
			ImPlot::PlotVLines("Current time", &current_time, 1);

			ImPlot::EndPlot();
		}
	}

	editor_->instrument_.SetFrequency(frequency_);

	if (ImGui::Button("Play audio"))
	{
		editor_->instrument_.Reset();

		editor_->instrument_.AddWave({
			.fundamental_wave = [&](float x) { return ImGui::CurveValueSmoothAudio(x, fundamental_wave_editor_data_.size(), fundamental_wave_editor_data_.data()); },
			//.fundamental_wave = pmk::wave::Sin01,
			.harmonic_multipliers = [&](float time, uint32_t freq_multiple) { return (1.0f / freq_multiple) * ImGui::CurveValueSmooth(freq_multiple / 16.0f, harmonics_editor_data_.size(), harmonics_editor_data_.data()); },
			});

		editor_->instrument_.Play();
	}

	ImGui::DragFloat("Frequency", &frequency_, 1.0f, 0.0f, 20000.0f);

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
