#pragma once

#include <functional>
#include "GLFW/glfw3.h"

#include "vulkan_renderer.h"
#include "scene.h"

struct EditorInfo
{
	VkImageView render_target;
	std::function<void()> gui_callback;
};

class Pumpkin
{
public:
	void Initialize();

	void Start();

	void MainLoop();

	void CleanUp();

	void SetEditorInfo(const EditorInfo* editor_info);

private:
	// General work the host needs to do each frame.
	void HostWork();

	// Work the host needs to do that modifies the render objects.
	void HostRenderWork();

	GLFWwindow* window_{};
	renderer::VulkanRenderer renderer_{};
	Scene scene_{};
	EditorInfo editor_info_{};

	uint32_t width_{ 800 };
	uint32_t height_{ 600 };
};
