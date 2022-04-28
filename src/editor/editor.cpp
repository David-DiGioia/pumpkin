#include "editor.h"

#include <string>
#include "imgui.h"

#include "gui.h"

void InitializeCallback(void* user_data)
{
	Editor* editor{ (Editor*)user_data };
	editor->InitializeGui();
}

// We pass rendered_image_id to the draw callback instead of at initialization because the
// rendered image ID is a frame resource, so we alternate which one gets drawn to each frame.
void GuiCallback(ImTextureID rendered_image_id, void* user_data)
{
	Editor* editor{ (Editor*)user_data };
	editor->DrawGui(rendered_image_id);
}

void Editor::Initialize()
{
}

void Editor::InitializeGui()
{
	gui_.Initialize();
}

void Editor::DrawGui(ImTextureID rendered_image_id)
{
	gui_.DrawGui(rendered_image_id);
}

renderer::EditorInfo Editor::GetEditorInfo()
{
	return {
		.initialization_callback = InitializeCallback,
		.gui_callback = GuiCallback,
		.user_data = (void*)this,
	};
}
