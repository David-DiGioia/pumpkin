#include "gui.h"

#include "imgui.h"

#include "logger.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

void InitializeGui()
{
	// Set ImGui settings.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = NULL; // Disable automatic saving of layout.
	io.IniSavingRate = 0.1f; // Small number so we can save often.
	ImGui::LoadIniSettingsFromDisk(default_layout_path.c_str());
}

void MainMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
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

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void MainGui()
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	MainMenu();

	if (!ImGui::Begin("Custom Window"))
	{
		ImGui::Text("Welcome to the Pumpkin engine!");

		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	//ImGui::ShowDemoWindow();

	ImGui::End();
}
