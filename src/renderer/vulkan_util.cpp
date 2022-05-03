#include "vulkan_util.h"

#include "logger.h"

namespace renderer
{
	// Stateless ---------------------------------------------------------------------------------------------

	void CheckResult(VkResult result, const std::string& msg)
	{
		if (result != VK_SUCCESS) {
			logger::Error("Vulkan function returned the VkResult: %d\nMessage: %s\n", result, msg.c_str());
		}
	}

	void PipelineBarrier(
		VkCommandBuffer cmd, VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask
	)
	{
		VkImageMemoryBarrier image_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = src_access_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
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
			src_stage_mask, // srcStageMask
			dst_stage_mask, // dstStageMask
			0,
			0,
			nullptr,
			0,
			nullptr,
			1, // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
	}

	// Stateful ----------------------------------------------------------------------------------------------

	void VulkanUtil::Initialize(Context* context, Allocator* alloc)
	{
		context_ = context;
		alloc_ = alloc;

		VkCommandPoolCreateInfo command_pool_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = context_->GetGraphicsQueueFamilyIndex(),
		};

		auto result{ vkCreateCommandPool(context_->device, &command_pool_info, nullptr, &command_pool_) };
		CheckResult(result, "Failed to create command pool.");

		VkCommandBufferAllocateInfo alloc_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool_,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		result = vkAllocateCommandBuffers(context_->device, &alloc_info, &cmd_);
		CheckResult(result, "Failed to allocate command buffer.");

		VkFenceCreateInfo fence_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = 0,
		};

		vkCreateFence(context_->device, &fence_info, nullptr, &fence_);
	}

	void VulkanUtil::PipelineBarrier(
		VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask
	)
	{
		renderer::PipelineBarrier(cmd_, image, old_layout, new_layout, src_access_mask, dst_access_mask, src_stage_mask, dst_stage_mask);
	}

	void VulkanUtil::CleanUp()
	{
		for (BufferResource& resource : destroy_queue_) {
			alloc_->DestroyBufferResource(&resource);
		}

		vkDestroyFence(context_->device, fence_, nullptr);
		vkDestroyCommandPool(context_->device, command_pool_, nullptr);
	}

	VkCommandBuffer& VulkanUtil::Begin()
	{
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		vkBeginCommandBuffer(cmd_, &begin_info);

		return cmd_;
	}

	void VulkanUtil::Submit()
	{
		vkEndCommandBuffer(cmd_);

		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd_,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr,
		};

		vkQueueSubmit(context_->graphics_queue, 1, &submit_info, fence_);

		VkResult result{ vkWaitForFences(context_->device, 1, &fence_, VK_TRUE, 1'000'000'000) };
		CheckResult(result, "Error waiting for render_fence.");
		result = vkResetFences(context_->device, 1, &fence_);
		CheckResult(result, "Error resetting render_fence.");

		for (BufferResource& resource : destroy_queue_) {
			alloc_->DestroyBufferResource(&resource);
		}
		destroy_queue_.clear();

		vkResetCommandPool(context_->device, command_pool_, 0);
	}
}
