#pragma once

#include "volk.h"
#include "GLFW/glfw3.h"

#include "context.h"
#include "swapchain.h"
#include "pipeline.h"

namespace renderer
{
	class VulkanRenderer
	{
	public:
		void Initialize(GLFWwindow* window);

		void CleanUp();

	private:
		Context context_{};
		Swapchain swapchain_{};
		GraphicsPipeline graphics_pipeline_{};
	};
}
