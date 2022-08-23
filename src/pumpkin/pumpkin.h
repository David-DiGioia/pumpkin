#pragma once

#include <chrono>
#include <filesystem>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "nlohmann/json.hpp"

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

		// Write render data info to json, and vertex data to a binary file.
		void DumpRenderData(nlohmann::json& j, const std::filesystem::path& binary_path) const;

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
