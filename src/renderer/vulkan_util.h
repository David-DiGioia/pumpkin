#pragma once

#include <string>
#include <filesystem>
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
	void CheckResult(VkResult result, const std::string& msg);

	std::string VkResultToString(VkResult result);

	VkTransformMatrixKHR ToVulkanTransformMatrix(const glm::mat4& mat);

	VkShaderModule LoadShaderModule(VkDevice device, const std::filesystem::path& path);

	VkDeviceAddress DeviceAddress(VkDevice device, VkBuffer buffer);

	template<typename T>
	void NameObject(VkDevice device, T handle, const std::string& name)
	{
		VkDebugUtilsObjectNameInfoEXT object_name_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = GetVulkanObjectType<T>(),
			.objectHandle = (uint64_t)handle,
			.pObjectName = name.c_str(),
		};

		VkResult result{ vkSetDebugUtilsObjectNameEXT(device, &object_name_info) };
		CheckResult(result, "Error setting debug util object name.");
	}

	// Get the lowest number we must add to offset such that it meets alignment requirement.
	template <typename T>
	T GetAlignmentOffset(T offset, T alignment)
	{
		return (alignment - (offset % alignment)) % alignment;
	}

	template <typename T>
	T AlignUp(T value, T alignment)
	{
		return value + GetAlignmentOffset(value, alignment);
	}

	void PipelineBarrier(
		VkCommandBuffer cmd, VkImage image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
		VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT
	);

	void PipelineBarrier(
		VkCommandBuffer cmd, VkBuffer buffer,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask
	);

	void PipelineBarrier(
		VkCommandBuffer cmd,
		VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
		VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask
	);

	// The "big hammer" barrier described in https://www.youtube.com/watch?v=JvAIdtAZnAw at 1 hour mark.
	// Makes all commands submitted before barrier finish execution / memory access entirely before
	// commands after barrier are executed. This should only be used for debugging, as it's inefficient.
	void PipelineBarrierBigHammer(VkCommandBuffer cmd);

	// Utility object to help with common Vulkan tasks that need a command buffer.
	class VulkanUtil
	{
	public:
		void Initialize(Context* context, Allocator* alloc);

		void TransferBufferToDevice(const void* host_buffer, uint32_t size, BufferResource& device_buffer);

		// Use staging buffer to transfer host memory to DEVICE_LOCAL buffer.
		template <typename T>
		void TransferBufferToDevice(const std::vector<T>& host_buffer, BufferResource& device_buffer)
		{
			TransferBufferToDevice(host_buffer.data(), host_buffer.size() * sizeof(T), device_buffer);
		}

		void PipelineBarrier(
			VkImage image,
			VkImageLayout old_layout, VkImageLayout new_layout,
			VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
			VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
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
	}

	template <typename T>
	VkObjectType GetVulkanObjectType()
	{
		if (std::is_same<VkInstance, T>::value) {
			return VK_OBJECT_TYPE_INSTANCE;
		}
		else if (std::is_same<VkPhysicalDevice, T>::value) {
			return VK_OBJECT_TYPE_PHYSICAL_DEVICE;
		}
		else if (std::is_same<VkDevice, T>::value) {
			return VK_OBJECT_TYPE_DEVICE;
		}
		else if (std::is_same<VkQueue, T>::value) {
			return VK_OBJECT_TYPE_QUEUE;
		}
		else if (std::is_same<VkSemaphore, T>::value) {
			return VK_OBJECT_TYPE_SEMAPHORE;
		}
		else if (std::is_same<VkCommandBuffer, T>::value) {
			return VK_OBJECT_TYPE_COMMAND_BUFFER;
		}
		else if (std::is_same<VkFence, T>::value) {
			return VK_OBJECT_TYPE_FENCE;
		}
		else if (std::is_same<VkDeviceMemory, T>::value) {
			return VK_OBJECT_TYPE_DEVICE_MEMORY;
		}
		else if (std::is_same<VkBuffer, T>::value) {
			return VK_OBJECT_TYPE_BUFFER;
		}
		else if (std::is_same<VkImage, T>::value) {
			return VK_OBJECT_TYPE_IMAGE;
		}
		else if (std::is_same<VkEvent, T>::value) {
			return VK_OBJECT_TYPE_EVENT;
		}
		else if (std::is_same<VkQueryPool, T>::value) {
			return VK_OBJECT_TYPE_QUERY_POOL;
		}
		else if (std::is_same<VkBufferView, T>::value) {
			return VK_OBJECT_TYPE_BUFFER_VIEW;
		}
		else if (std::is_same<VkImageView, T>::value) {
			return VK_OBJECT_TYPE_IMAGE_VIEW;
		}
		else if (std::is_same<VkShaderModule, T>::value) {
			return VK_OBJECT_TYPE_SHADER_MODULE;
		}
		else if (std::is_same<VkPipelineCache, T>::value) {
			return VK_OBJECT_TYPE_PIPELINE_CACHE;
		}
		else if (std::is_same<VkPipelineLayout, T>::value) {
			return VK_OBJECT_TYPE_PIPELINE_LAYOUT;
		}
		else if (std::is_same<VkRenderPass, T>::value) {
			return VK_OBJECT_TYPE_RENDER_PASS;
		}
		else if (std::is_same<VkPipeline, T>::value) {
			return VK_OBJECT_TYPE_PIPELINE;
		}
		else if (std::is_same<VkDescriptorSetLayout, T>::value) {
			return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
		}
		else if (std::is_same<VkSampler, T>::value) {
			return VK_OBJECT_TYPE_SAMPLER;
		}
		else if (std::is_same<VkDescriptorPool, T>::value) {
			return VK_OBJECT_TYPE_DESCRIPTOR_POOL;
		}
		else if (std::is_same<VkDescriptorSet, T>::value) {
			return VK_OBJECT_TYPE_DESCRIPTOR_SET;
		}
		else if (std::is_same<VkFramebuffer, T>::value) {
			return VK_OBJECT_TYPE_FRAMEBUFFER;
		}
		else if (std::is_same<VkCommandPool, T>::value) {
			return VK_OBJECT_TYPE_COMMAND_POOL;
		}
		else if (std::is_same<VkSamplerYcbcrConversion, T>::value) {
			return VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION;
		}
		else if (std::is_same<VkDescriptorUpdateTemplate, T>::value) {
			return VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE;
		}
		else if (std::is_same<VkPrivateDataSlot, T>::value) {
			return VK_OBJECT_TYPE_PRIVATE_DATA_SLOT;
		}
		else if (std::is_same<VkSurfaceKHR, T>::value) {
			return VK_OBJECT_TYPE_SURFACE_KHR;
		}
		else if (std::is_same<VkSwapchainKHR, T>::value) {
			return VK_OBJECT_TYPE_SWAPCHAIN_KHR;
		}
		else if (std::is_same<VkDisplayKHR, T>::value) {
			return VK_OBJECT_TYPE_DISPLAY_KHR;
		}
		else if (std::is_same<VkDisplayModeKHR, T>::value) {
			return VK_OBJECT_TYPE_DISPLAY_MODE_KHR;
		}
		else if (std::is_same<VkDebugReportCallbackEXT, T>::value) {
			return VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
		}
		else if (std::is_same<VkDebugUtilsMessengerEXT, T>::value) {
			return VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
		}
		else if (std::is_same<VkAccelerationStructureKHR, T>::value) {
			return VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
		}
		else if (std::is_same<VkValidationCacheEXT, T>::value) {
			return VK_OBJECT_TYPE_VALIDATION_CACHE_EXT;
		}
		else if (std::is_same<VkDeferredOperationKHR, T>::value) {
			return VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR;
		}
		else if (std::is_same<VkMicromapEXT, T>::value) {
			return VK_OBJECT_TYPE_MICROMAP_EXT;
		}

		logger::Error("Unknown Vulkan object type requested.");

		return VK_OBJECT_TYPE_UNKNOWN;
	}
}
