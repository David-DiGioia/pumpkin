#include "gui.h"

#include "imgui.h"

#include "logger.h"

const std::string default_layout_path{ "default_imgui_layout.ini" };

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
}

void EditorGui::DrawGui(ImTextureID* rendered_image_id)
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	MainMenu();
	RightPane();
	EngineViewport(rendered_image_id);
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
	if (!ImGui::Begin("Custom Window"))
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
	bool success{ ImGui::Begin("Custom Window") };

	ImVec2 render_size{ ImGui::GetContentRegionAvail() };
	UpdateViewportSize({ (uint32_t)render_size.x, (uint32_t)render_size.y });

	if (!success)
	{
		ImGui::End();
		return;
	}

	ImGui::Image(*rendered_image_id, render_size);

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
