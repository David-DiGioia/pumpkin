#include "vulkan_util.h"

#include "logger.h"

namespace renderer
{
	void CheckResult(VkResult result, const std::string& msg)
	{
		if (result != VK_SUCCESS) {
			logger::Error("Vulkan function returned the VkResult: %d\nMessage: %s\n", result, msg.c_str());
		}
	}

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

	void VulkanUtil::CleanUp()
	{
		for (BufferResource& resource : destroy_queue_) {
			alloc_->DestroyBufferResource(&resource);
		}

		vkDestroyFence(context_->device, fence_, nullptr);
		vkDestroyCommandPool(context_->device, command_pool_, nullptr);
	}

	void VulkanUtil::Begin()
	{
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		vkBeginCommandBuffer(cmd_, &begin_info);
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

	VkCommandBuffer& VulkanUtil::GetCommandBuffer()
	{
		return cmd_;
	}
}
