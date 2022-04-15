#pragma once

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

private:
	GLFWwindow* window_{};
	renderer::VulkanRenderer renderer_{};
	Scene scene_{};

	uint32_t width_{ 800 };
	uint32_t height_{ 600 };
};
