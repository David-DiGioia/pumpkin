#pragma once

#include "GLFW/glfw3.h"

#include "vulkan_renderer.h"
#include "scene.h"

namespace pmk
{
	class Pumpkin
	{
	public:
		void Initialize();

		void Start();

		void MainLoop();

		void CleanUp();

		void SetImGuiCallbacksInfo(const renderer::ImGuiCallbacks& editor_info);

		void SetEditorViewportSize(const renderer::Extent& extent);

	private:
		// General work the host needs to do each frame.
		void HostWork();

		// Work the host needs to do that modifies the render objects.
		void HostRenderWork();

		GLFWwindow* window_{};
		renderer::VulkanRenderer renderer_{};
		Scene scene_{};

		uint32_t width_{ 1280 };
		uint32_t height_{ 720 };
	};
}
