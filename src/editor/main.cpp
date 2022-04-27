#include "pumpkin.h"
#include "editor.h"

#include "volk.h"

int main()
{
	Editor editor{};
	editor.Initialize();

	pmk::Pumpkin pumpkin{};
	pumpkin.SetEditorInfo(editor.GetEditorInfo());
	pumpkin.Start();

	return 0;
}
