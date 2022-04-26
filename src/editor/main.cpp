#include "pumpkin.h"
#include "editor.h"

#include "volk.h"

int main()
{
	Editor editor{};
	editor.Initialize();

	auto render_callback{ editor.GetRenderCallback() };

	Pumpkin pumpkin{};
	pumpkin.SetEditorCallback(render_callback);
	pumpkin.Start();

	return 0;
}
