#pragma once

#include <string>
#include "glm/glm.hpp"
#include "volk.h"

#include "logger.h"
#include "memory_allocator.h"
#include "renderer_types.h"

#define VERTEX_ATTRIBUTE(loc, attr)								\
	VkVertexInputAttributeDescription{							\
		.location = loc,										\
		.binding = VERTEX_BINDING,								\
		.format = GetVulkanFormat<decltype(Vertex::attr)>(),	\
		.offset = offsetof(Vertex, attr),						\
	}

namespace renderer
{
	template <typename T>
	VkFormat GetVulkanFormat()
	{
		if (std::is_same<glm::vec4, T>::value) {
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		else if (std::is_same<glm::vec3, T>::value) {
			return VK_FORMAT_R32G32B32_SFLOAT;
		}
		else if (std::is_same<glm::vec2, T>::value) {
			return VK_FORMAT_R32G32_SFLOAT;
		}
		else if (std::is_same<float, T>::value) {
			return VK_FORMAT_R32_SFLOAT;
		}
		else if (std::is_same<glm::ivec4, T>::value) {
			return VK_FORMAT_R32G32B32A32_SINT;
		}
		else if (std::is_same<glm::ivec3, T>::value) {
			return VK_FORMAT_R32G32B32_SINT;
		}
		else if (std::is_same<glm::ivec2, T>::value) {
			return VK_FORMAT_R32G32_SINT;
		}
		else if (std::is_same<int, T>::value) {
			return VK_FORMAT_R32_SINT;
		}
		else if (std::is_same<glm::uvec4, T>::value) {
			return VK_FORMAT_R32G32B32A32_UINT;
		}
		else if (std::is_same<glm::uvec3, T>::value) {
			return VK_FORMAT_R32G32B32_UINT;
		}
		else if (std::is_same<glm::uvec2, T>::value) {
			return VK_FORMAT_R32G32_UINT;
		}
		else if (std::is_same<uint32_t, T>::value) {
			return VK_FORMAT_R32_UINT;
		}

		logger::Error("Unknown Vulkan format requested.");

		return VK_FORMAT_UNDEFINED;
	}

	void CheckResult(VkResult result, const std::string& msg);

	void PipelineBarrier(
		VkCommandBuffer cmd, VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT
	);

	// Utility object to help with common Vulkan tasks that need a command buffer.
	class VulkanUtil
	{
	public:
		void Initialize(Context* context, Allocator* alloc);

		// Use staging buffer to transfer host memory to DEVICE_LOCAL buffer.
		template <typename T>
		void TransferBufferToDevice(const std::vector<T>& host_buffer, BufferResource& device_buffer)
		{
			BufferResource staging{ alloc_->CreateBufferResource(host_buffer.size() * sizeof(T),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };

			// Copy data to staging buffer.
			void* data{};
			vkMapMemory(context_->device, *staging.memory, staging.offset, staging.size, 0, &data);
			memcpy(data, host_buffer.data(), staging.size);
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

		void PipelineBarrier(
			VkImage image,
			VkImageLayout old_layout, VkImageLayout new_layout,
			VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
			VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
			VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT
		);

		void CleanUp();

		// Call this before issuing any commands.
		// Use the command buffer to do custom commands between calling Begin() and Submit().
		VkCommandBuffer& Begin();

		// Call this to submit all commands and wait for them to finish.
		void Submit();

	private:
		Context* context_{};
		Allocator* alloc_{};
		VkCommandPool command_pool_{};
		VkCommandBuffer cmd_{};
		std::vector<BufferResource> destroy_queue_{}; // Staging buffers are destroyed after each submit.
		VkFence fence_{};
	};
}