#pragma once

#include "GLFW/glfw3.h"
#include "vulkan_renderer.h"

class Pumpkin
{
public:
	void Initialize();

	void Start();

	void MainLoop();

	void CleanUp();

private:
	GLFWwindow* window_{};
	renderer::VulkanRenderer renderer_{};

	uint32_t width_{};
	uint32_t height_{};
};
