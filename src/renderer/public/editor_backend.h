#pragma once

#include <functional>
#include "imgui.h"

namespace renderer
{
	struct EditorInfo
	{
		std::function<void(ImTextureID rendered_image_id, void* user_data)> initialization_callback{};
		std::function<void(void)> gui_callback{};
		void* user_data; // Passed to initialize_callback.
	};
}
