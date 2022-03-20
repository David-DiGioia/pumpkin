#include "vulkan_renderer.h"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

#include "vulkan_util.h"
#include "logger.h"
#include "context.h"

namespace renderer
{
	void VulkanRenderer::Initialize(GLFWwindow* window)
	{
		VkResult result{ volkInitialize() };
		CheckResult(result, "Failed to initialize volk.");

		context_.Initialize(window);
		swapchain_.Initialize(&context_);
		graphics_pipeline_.Initialize(&context_, &swapchain_);

		InitializeFrameResources();
	}

	void VulkanRenderer::CleanUp()
	{
		swapchain_.CleanUp();
		context_.CleanUp();
		graphics_pipeline_.CleanUp();
	}

	void VulkanRenderer::Present()
	{

	}

	FrameResources& VulkanRenderer::GetCurrentFrame()
	{
		return frame_resources_[current_frame_];
	}

	void VulkanRenderer::InitializeFrameResources()
	{
		InitializeCommandBuffers();
		InitializeSyncObjects();
	}

	void VulkanRenderer::InitializeCommandBuffers()
	{
		VkCommandPoolCreateInfo pool_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = context_.GetGraphicsQueueFamilyIndex(),
		};

		VkResult result{ vkCreateCommandPool(context_.device, &pool_info, nullptr, &command_pool_) };
		CheckResult(result, "Failed to create command pool.");

		VkCommandBufferAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool_,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		for (uint32_t i{ 0 }; i < FRAMES_IN_FLIGHT; ++i)
		{
			VkCommandBuffer* cmd_buffer{ &frame_resources_[i].command_buffer };
			VkResult result{ vkAllocateCommandBuffers(context_.device, &allocate_info, cmd_buffer) };
			CheckResult(result, "Failed to allocate command buffer.");
		}
	}

	void VulkanRenderer::InitializeSyncObjects()
	{

	}
}
