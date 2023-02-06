#include "vulkan_util.h"

#include <fstream>

#include "logger.h"

namespace renderer
{
	// Stateless ---------------------------------------------------------------------------------------------

	void CheckResult(VkResult result, const std::string& msg)
	{
		if (result != VK_SUCCESS)
		{
			std::string str{ VkResultToString(result) };
			logger::Error("Vulkan function returned the VkResult: %d (%s)\nMessage: %s\n", result, str.c_str(), msg.c_str());
		}
	}

	std::string VkResultToString(VkResult result)
	{
		switch (result)
		{
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_NOT_READY:
			return "VK_NOT_READY";
		case VK_TIMEOUT:
			return "VK_TIMEOUT";
		case VK_EVENT_SET:
			return "VK_EVENT_SET";
		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";
		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL:
			return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN:
			return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:
			return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION:
			return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
			return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_PIPELINE_COMPILE_REQUIRED:
			return "VK_PIPELINE_COMPILE_REQUIRED";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV:
			return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
			return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		case VK_ERROR_NOT_PERMITTED_KHR:
			return "VK_ERROR_NOT_PERMITTED_KHR";
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
			return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		case VK_THREAD_IDLE_KHR:
			return "VK_THREAD_IDLE_KHR";
		case VK_THREAD_DONE_KHR:
			return "VK_THREAD_DONE_KHR";
		case VK_OPERATION_DEFERRED_KHR:
			return "VK_OPERATION_DEFERRED_KHR";
		case VK_OPERATION_NOT_DEFERRED_KHR:
			return "VK_OPERATION_NOT_DEFERRED_KHR";
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
			return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
		default:
			return "Unrecognized enum";
		}
	}

	VkTransformMatrixKHR ToVulkanTransformMatrix(const glm::mat4& mat)
	{
		// GLM is column major, but Vulkan matrix is row major, so we transpose.
		glm::mat4 transposed{ glm::transpose(mat) };
		// Vulkan matrix is row 3x4, so only copy that part of mat.
		VkTransformMatrixKHR result{};
		std::memcpy(result.matrix, &transposed, sizeof(float) * 3 * 4);
		return result;
	}

	VkShaderModule LoadShaderModule(VkDevice device, const std::filesystem::path& path)
	{
		// std::ios::ate puts cursor at end of file upon opening.
		std::ifstream file{ path, std::ios::ate | std::ios::binary };

		if (!file.is_open()) {
			logger::Error("Can't open file: %s\n", path.c_str());
		}

		// Find size of file in bytes by position of cursor.
		size_t file_size{ (size_t)file.tellg() };

		// Spirv expects the buffer to be a uint32, so make sure to
		// reserve a vector big enough for the entire file.
		std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

		// Put file cursor at beginning.
		file.seekg(0);

		// Load the entire file into the buffer.
		file.read((char*)buffer.data(), file_size);

		file.close();

		// Create a new shader module using the buffer we loaded.
		VkShaderModuleCreateInfo module_info{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.flags = 0,
			.codeSize = buffer.size() * sizeof(uint32_t),
			.pCode = buffer.data(),
		};

		VkShaderModule shader_module;
		VkResult result{ vkCreateShaderModule(device, &module_info, nullptr, &shader_module) };
		CheckResult(result, "Failed to create shader module.");
		NameObject(device, shader_module, path.string());

		return shader_module;
	}

	VkDeviceAddress DeviceAddress(VkDevice device, VkBuffer buffer)
	{
		VkBufferDeviceAddressInfo device_address_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = buffer,
		};

		return vkGetBufferDeviceAddress(device, &device_address_info);
	}

	void PipelineBarrier(
		VkCommandBuffer cmd, VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkImageAspectFlags image_aspect)
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
			  .aspectMask = image_aspect,
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
			1,                    // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
	}

	void PipelineBarrier(
		VkCommandBuffer cmd, VkBuffer buffer,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask)
	{
		VkBufferMemoryBarrier buffer_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = src_access_mask,
			.dstAccessMask = dst_access_mask,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		};

		vkCmdPipelineBarrier(
			cmd,
			src_stage_mask, // srcStageMask
			dst_stage_mask, // dstStageMask
			0,
			0,
			nullptr,
			1,                      // bufferMemoryBarrierCount
			&buffer_memory_barrier, //pBufferMemoryBarriers
			0,
			nullptr
		);
	}

	void PipelineBarrier(VkCommandBuffer cmd,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask)
	{
		VkMemoryBarrier memory_barrier{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = src_access_mask,
			.dstAccessMask = dst_access_mask,
		};

		vkCmdPipelineBarrier(
			cmd,
			src_stage_mask, // srcStageMask
			dst_stage_mask, // dstStageMask
			0,
			1,               // memoryBarrierCount
			&memory_barrier, // pMemoryBarriers
			0,
			nullptr,
			0,
			nullptr
		);
	}

	void PipelineBarrierBigHammer(VkCommandBuffer cmd)
	{
		VkMemoryBarrier memory_barrier{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		};

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // srcStageMask
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
			0,
			1, // memoryBarrierCount
			&memory_barrier, // pMemoryBarriers
			0,
			nullptr,
			0,
			nullptr
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
		NameObject(context_->device, command_pool_, "Vulkan_Util_Command_pool");

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
		NameObject(context_->device, fence_, "Vulkan_Util_Fence");
	}

	void VulkanUtil::TransferBufferToDevice(const void* host_buffer, uint32_t size, BufferResource& device_buffer)
	{
		BufferResource staging{ alloc_->CreateBufferResource(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };
		NameObject(context_->device, staging.buffer, std::string{ "Vulkan_Util_Transfer_Staging_Buffer" });

		// Copy data to staging buffer.
		void* data{};
		vkMapMemory(context_->device, *staging.memory, staging.offset, staging.size, 0, &data);
		memcpy(data, host_buffer, staging.size);
		vkUnmapMemory(context_->device, *staging.memory);

		// Transfer from staging to device.
		VkBufferCopy buffer_copy{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = staging.size,
		};

		vkCmdCopyBuffer(cmd_, staging.buffer, device_buffer.buffer, 1, &buffer_copy);

		destroy_queue_.push_back(staging);
	}

	void VulkanUtil::PipelineBarrier(
		VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkImageAspectFlags image_aspect)
	{
		renderer::PipelineBarrier(cmd_, image, old_layout, new_layout, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, image_aspect);
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

		VkResult result{ vkQueueSubmit(context_->graphics_queue, 1, &submit_info, fence_) };
		CheckResult(result, "Error submitting VulkanUtil queue.\n");

		// TODO: Maybe at some pointer, return fence instead so it can be optionally waited on,
		//       or the caller can use barriers to synchronize with later queue submissions.
		result = vkWaitForFences(context_->device, 1, &fence_, VK_TRUE, 1'000'000'000);
		CheckResult(result, "Error waiting for VulkanUtil render_fence.");
		result = vkResetFences(context_->device, 1, &fence_);
		CheckResult(result, "Error resetting VulkanUtil render_fence.");

		for (BufferResource& resource : destroy_queue_) {
			alloc_->DestroyBufferResource(&resource);
		}
		destroy_queue_.clear();

		result = vkResetCommandPool(context_->device, command_pool_, 0);
		CheckResult(result, "Error resetting VulkanUtil command pool.");
	}
}
