#pragma once

#include <chrono>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

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

		void SetCameraPosition(const glm::vec3& pos);

		Scene& GetScene();

		float GetDeltaTime() const;

	private:
		// General work the host needs to do each frame.
		void HostWork();

		// Work the host needs to do that modifies the render objects.
		void HostRenderWork();

		void UpdateDeltaTime();

		GLFWwindow* window_{};
		renderer::VulkanRenderer renderer_{};
		Scene scene_{};

		float delta_time_{};
		std::chrono::steady_clock::time_point last_time_{};

		uint32_t width_{ 1920 };
		uint32_t height_{ 1080 };
	};
}
