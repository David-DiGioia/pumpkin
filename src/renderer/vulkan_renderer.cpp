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
		allocator_.Initialize(context_.device);

		InitializeFrameResources();
	}

	void VulkanRenderer::CleanUp()
	{
		swapchain_.CleanUp();
		context_.CleanUp();
		graphics_pipeline_.CleanUp();
	}

	void VulkanRenderer::Render()
	{
		// Wait until the GPU has finished rendering the last frame to use these resources. Timeout of 1 second.
		VkResult result{ vkWaitForFences(context_.device, 1, &GetCurrentFrame().render_done_fence, true, 1'000'000'000) };
		CheckResult(result, "Error waiting for render_fence.");
		result = vkResetFences(context_.device, 1, &GetCurrentFrame().render_done_fence);
		CheckResult(result, "Error resetting render_fence.");

		// Request image from the swapchain, one second timeout.
		// This is also where vsync happens according to vkguide, but for me it happens at present.
		uint32_t image_index{ swapchain_.AcquireNextImage(GetCurrentFrame().image_acquired_semaphore) };

		// Now that we are sure that the commands finished executing, we can safely
		// reset the command buffer to begin recording again.
		result = vkResetCommandBuffer(GetCurrentFrame().command_buffer, 0);
		CheckResult(result, "Error resetting command buffer.");

		// Drawing commands happen here.
		RecordCommandBuffer(GetCurrentFrame().command_buffer, image_index);

		VkPipelineStageFlags wait_stage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		// We can't start rendering until image_acquired_semaphore is signaled,
		// meaning the image is ready to be used.
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &GetCurrentFrame().image_acquired_semaphore,
			.pWaitDstStageMask = &wait_stage,
			.commandBufferCount = 1,
			.pCommandBuffers = &GetCurrentFrame().command_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &GetCurrentFrame().render_done_semaphore,
		};

		// Submit command buffer to the queue and execute it.
		// render_done_fence will now block until the graphic commands finish execution.
		result = vkQueueSubmit(context_.graphics_queue, 1, &submit_info, GetCurrentFrame().render_done_fence);
		CheckResult(result, "Error submitting queue.");

		// This will put the image we just rendered into the visible window.
		// We want to wait on render_done_semaphore for that, as it's necessary that the
		// drawing commands have finished before the image is displayed to the user.
		VkPresentInfoKHR present_info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &GetCurrentFrame().render_done_semaphore,
			.swapchainCount = 1,
			.pSwapchains = &swapchain_.GetSwapchain(),
			.pImageIndices = &image_index,
			.pResults = nullptr,
		};

		// This is what blocks for vsync on my gtx 1080.
		result = vkQueuePresentKHR(context_.graphics_queue, &present_info);
		CheckResult(result, "Error presenting image.");

		NextFrame();
	}

	void VulkanRenderer::Draw(VkCommandBuffer cmd, uint32_t image_index)
	{
		Extents window_extents{ context_.GetWindowExtents() };
		VkClearColorValue clear_color{ 1.0f, 0.0f, 0.0f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = swapchain_.GetImageView(image_index),
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_color,
		};

		VkRenderingInfo rendering_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = 0,
			.renderArea = {
				.offset = {0, 0},
				.extent = {window_extents.width, window_extents.height},
			},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr,
		};

		// First render pass.
		vkCmdBeginRendering(cmd, &rendering_info);



		vkCmdEndRendering(cmd);
	}

	void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index)
	{
		// Begin the command buffer recording. We will use this command buffer exactly once,
		// so we want to let Vulkan know that.
		VkCommandBufferBeginInfo command_buffer_begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		VkResult result{ vkBeginCommandBuffer(cmd, &command_buffer_begin_info) };
		CheckResult(result, "Failed to begin command buffer.");

		TransitionSwapImageForRender(cmd, image_index);
		Draw(cmd, image_index);
		TransitionSwapImageForPresent(cmd, image_index);

		result = vkEndCommandBuffer(GetCurrentFrame().command_buffer);
		CheckResult(result, "Failed to end command buffer.");
	}

	void VulkanRenderer::TransitionSwapImageForRender(VkCommandBuffer cmd, uint32_t image_index)
	{
		// Transition image to optimal layout for rendering to.
		VkImageMemoryBarrier image_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain_.GetImage(image_index),
			.subresourceRange = {
			  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			  .baseMipLevel = 0,
			  .levelCount = 1,
			  .baseArrayLayer = 0,
			  .layerCount = 1,
			}
		};

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
			0,
			0,
			nullptr,
			0,
			nullptr,
			1, // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
	}

	void VulkanRenderer::TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index)
	{
		// Transition image to optimal format for presentation.
		VkImageMemoryBarrier image_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain_.GetImage(image_index),
			.subresourceRange = {
			  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			  .baseMipLevel = 0,
			  .levelCount = 1,
			  .baseArrayLayer = 0,
			  .layerCount = 1,
			}
		};

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
			VK_DEPENDENCY_BY_REGION_BIT,
			0,
			nullptr,
			0,
			nullptr,
			1, // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
	}

	void VulkanRenderer::NextFrame()
	{
		current_frame_ = (current_frame_ + 1) % FRAMES_IN_FLIGHT;
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
		// Start in signaled state since frame resources are initially not in use.
		VkFenceCreateInfo fence_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		VkSemaphoreCreateInfo semaphore_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.flags = 0 // Reserved.
		};

		for (FrameResources& resource : frame_resources_)
		{
			VkResult result{ vkCreateFence(context_.device, &fence_info, nullptr, &resource.render_done_fence) };
			CheckResult(result, "Failed to create fence.");

			result = vkCreateSemaphore(context_.device, &semaphore_info, nullptr, &resource.image_acquired_semaphore);
			CheckResult(result, "Failed to create semaphore.");

			result = vkCreateSemaphore(context_.device, &semaphore_info, nullptr, &resource.render_done_semaphore);
			CheckResult(result, "Failed to create semaphore.");
		}
	}
}
