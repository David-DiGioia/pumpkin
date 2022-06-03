#include "pumpkin.h"
#include "editor.h"

#include "volk.h"

int main()
{
	pmk::Pumpkin pumpkin{};
	Editor editor{};
	editor.Initialize(&pumpkin);

	pumpkin.SetImGuiCallbacksInfo(editor.GetEditorInfo());
	pumpkin.Start();

	return 0;
}
