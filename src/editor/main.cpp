#include "pumpkin.h"

#include "volk.h"

int main()
{
	Editor editor{};
	editor.Initialize();

	EditorInfo editor_info{ editor.GetEditorInfo() };

	Pumpkin pumpkin{};
	pumpkin.SetEditorInfo(&editor_info);
	pumpkin.Start();

	return 0;
}
