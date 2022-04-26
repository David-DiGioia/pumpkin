#pragma once

#include <functional>
#include "GLFW/glfw3.h"

#include "vulkan_renderer.h"
#include "scene.h"

class Pumpkin
{
public:
	void Initialize();

	void Start();

	void MainLoop();

	void CleanUp();

	void SetEditorCallback(std::function<void(void)> callback);

private:
	// General work the host needs to do each frame.
	void HostWork();

	// Work the host needs to do that modifies the render objects.
	void HostRenderWork();

	GLFWwindow* window_{};
	renderer::VulkanRenderer renderer_{};
	Scene scene_{};

	bool editor_mode_enabled_{ false };
	uint32_t width_{ 800 };
	uint32_t height_{ 600 };
};
