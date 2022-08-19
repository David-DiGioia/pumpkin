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

	pumpkin.Start();

	editor.CleanUp();
	pumpkin.CleanUp();
	return 0;
}
