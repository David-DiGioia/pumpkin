#include "editor.h"

void RenderCallback()
{

}

void Editor::Initialize()
{
}

std::function<void(void)> Editor::GetRenderCallback()
{
	return RenderCallback;
}
