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

	private:
		Context context_{};
	};
}
