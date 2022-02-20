#pragma once

#include "volk.h"
#include "GLFW/glfw3.h"
#include "init.h"

namespace renderer
{
	class VulkanRenderer
	{
	public:
		void Initialize();

		void CleanUp();

	private:
		Context context_{};
	};
}
