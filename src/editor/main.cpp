#include "pumpkin.h"
#include "editor.h"

#include "volk.h"

int main()
{
	pmk::Pumpkin pumpkin{};
	Editor editor{};
	pumpkin.SetImGuiCallbacksInfo(editor.GetEditorInfo());
	pumpkin.Initialize();

	editor.Initialize(&pumpkin);

	editor.ImportGLTF("test_gltf.gltf"); // TODO: Proper import through editor GUI.

	pumpkin.Start();

	return 0;
}
