#include "editor.h"

#include <string>
#include "imgui.h"

#include "gui.h"

void InitializeCallback(ImTextureID rendered_image_id, void* user_data)
{
	Editor* editor{ (Editor*)user_data };
	editor->SetRenderedImageID(rendered_image_id);

	InitializeGui();
}

void Editor::Initialize()
{
}

void Editor::SetRenderedImageID(ImTextureID rendered_image_id)
{
	rendered_image_id_ = rendered_image_id;
}

renderer::EditorInfo Editor::GetEditorInfo()
{
	return {
		.initialization_callback = InitializeCallback,
		.gui_callback = MainGui,
		.user_data = (void*)this,
	};
}
