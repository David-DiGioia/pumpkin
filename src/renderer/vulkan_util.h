#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <array>
#include <functional>
#include <unordered_map>
#include "glm/glm.hpp"
#include "volk.h"

#include "logger.h"
#include "memory_allocator.h"
#include "renderer_types.h"
#include "renderer_constants.h"
#include "render_object.h"	
#include "common_constants.h"

#define VERTEX_ATTRIBUTE(loc, attr)								\
	VkVertexInputAttributeDescription{							\
		.location = loc,										\
		.binding = VERTEX_BINDING,								\
		.format = GetVulkanFormat<decltype(Vertex::attr)>(),	\
		.offset = offsetof(Vertex, attr),						\
	}

#ifdef EDITOR_ENABLED
#define MPM_PARTICLE_ATTRIBUTE(loc, attr)										\
	VkVertexInputAttributeDescription{											\
		.location = loc,														\
		.binding = INSTANCE_BINDING,											\
		.format = GetVulkanFormat<decltype(MPMDebugParticleInstance::attr)>(),	\
		.offset = offsetof(MPMDebugParticleInstance, attr),						\
	}

#define MPM_NODE_ATTRIBUTE(loc, attr)										\
	VkVertexInputAttributeDescription{										\
		.location = loc,													\
		.binding = INSTANCE_BINDING,										\
		.format = GetVulkanFormat<decltype(MPMDebugNodeInstance::attr)>(),	\
		.offset = offsetof(MPMDebugNodeInstance, attr),						\
	}

#define RIGID_BODY_VOXEL_ATTRIBUTE(loc, attr)										\
	VkVertexInputAttributeDescription{												\
		.location = loc,															\
		.binding = INSTANCE_BINDING,												\
		.format = GetVulkanFormat<decltype(RigidBodyDebugVoxelInstance::attr)>(),	\
		.offset = offsetof(RigidBodyDebugVoxelInstance, attr),						\
	}
#endif

namespace renderer
{
	class RenderObjectDestroyer
	{
	public:
		void Initialize(
			std::array<std::vector<RenderObject*>*, FRAMES_IN_FLIGHT>&& frame_resource,
			std::function<void(RenderObject*, bool)> destroyer_func,
			int32_t current_frame)
		{
			frame_resource_ = std::move(frame_resource);
			destroyer_func_ = destroyer_func;
			current_frame_ = current_frame;
		}

		void DestroyElement(uint32_t index)
		{
			// Do nothing for null render objects.
			if (index == NULL_INDEX || !frame_resource_[0]->at(index)) {
				return;
			}

			DestroyElementCurrentFrame(index, false);

			// We've immediately destroyed the current frame's render object, but we queue the other ones to be destroyed next frame.
			std::array<RenderObject*, FRAMES_IN_FLIGHT - 1> other_render_objects{};
			uint32_t j{ 0 };
			for (uint32_t i{ 0 }; i < FRAMES_IN_FLIGHT; ++i)
			{
				if (i == current_frame_) {
					continue;
				}
				std::vector<RenderObject*>& vec{ *frame_resource_[i] };
				other_render_objects[j++] = vec[index];

				// Set all frame's resource to null since we've copied them here.
				// From the renderer's point of view, they've all been deleted already.
				vec[index] = nullptr;
			}
			destruction_queue_.push_back({ index, other_render_objects });
		}

		uint32_t PopVacantMeshIndex()
		{
			if (vacant_mesh_indices_.empty()) {
				return NULL_INDEX;
			}

			uint32_t back{ vacant_mesh_indices_.back() };
			vacant_mesh_indices_.pop_back();
			return back;
		}

		// Call once per frame.
		void FrameUpdate()
		{
			for (auto& [ro_index, render_objects] : destruction_queue_)
			{
				vacant_mesh_indices_.push_back(render_objects[0]->mesh_idx);
				DestroyQueuedElement(render_objects);
			}
			destruction_queue_.clear();
		}

		void NextFrame()
		{
			current_frame_ = (current_frame_ + 1) % FRAMES_IN_FLIGHT;
		}

		// Should only be called when new project is loaded.
		void SetVacantMeshIndices(std::vector<uint32_t>&& mesh_indices)
		{
			vacant_mesh_indices_ = std::move(mesh_indices);
		}

	private:
		void DestroyElementCurrentFrame(uint32_t index, bool last_resource)
		{
			std::vector<RenderObject*>& vec{ *frame_resource_[current_frame_] };
			destroyer_func_(vec[index], last_resource);
			vec[index] = nullptr;
		}

		void DestroyQueuedElement(std::array<RenderObject*, FRAMES_IN_FLIGHT - 1>& render_objects)
		{
			uint32_t i{ 0 };
			for (RenderObject* ro_ptr : render_objects)
			{
				bool last_element{ i == FRAMES_IN_FLIGHT - 2 }; // Since destruction queue element has FRAMES_IN_FLIGHT - 1 elements.
				destroyer_func_(ro_ptr, last_element);
				++i;
			}
		}

		// Returns true if the element is destroyed in all but the current frame.
		bool ElementDestroyedInAllOtherFrames(uint32_t index)
		{
			std::vector<RenderObject*>& vec{ *frame_resource_[current_frame_] };

			for (uint32_t frame_idx{ 0 }; frame_idx < FRAMES_IN_FLIGHT; ++frame_idx)
			{
				if (current_frame_ == frame_idx) {
					continue;
				}
				if (vec[frame_idx]) {
					return false;
				}
			}
			return true;
		}

		std::array<std::vector<RenderObject*>*, FRAMES_IN_FLIGHT> frame_resource_{}; // Points to renderer's resources so it is up to date.
		std::function<void(RenderObject*, bool)> destroyer_func_{};
		uint32_t current_frame_{};

		std::vector<std::pair<uint32_t, std::array<RenderObject*, FRAMES_IN_FLIGHT - 1>>> destruction_queue_{}; // Indices queued for destruction. Pairs of (ro_idx, render objects).
		std::vector<uint32_t> vacant_mesh_indices_{};                                                           // Destroyed indices ready to be reused.
	};

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
	private:
		struct FrameResources;

	public:

		void Initialize(Context* context, Allocator* alloc);

		void TransferBufferToDevice(const void* host_buffer, uint32_t size, BufferResource& device_buffer);

		// Like TransferBufferToDevice(...) but recording to a custom command buffer cmd.
		// Useful to prevent needing CPU to wait on command to finish to be used in graphics pipeline.
		void TransferBufferToDeviceCmd(VkCommandBuffer cmd, const void* host_buffer, uint32_t size, BufferResource& device_buffer);

		// Use staging buffer to transfer host memory to DEVICE_LOCAL buffer.
		template <typename T>
		void TransferBufferToDevice(const std::vector<T>& host_buffer, BufferResource& device_buffer)
		{
			TransferBufferToDevice(host_buffer.data(), (uint32_t)host_buffer.size() * sizeof(T), device_buffer);
		}

		void TransferBufferToHost(void* host_buffer, uint32_t size, const BufferResource& device_buffer);

		// Use staging buffer to transfer host memory to DEVICE_LOCAL buffer.
		template <typename T>
		void TransferBufferToHost(std::vector<T>& host_buffer, const BufferResource& device_buffer)
		{
			TransferBufferToHost(host_buffer.data(), (uint32_t)host_buffer.size() * sizeof(T), device_buffer);
		}

		void TransferImageToDevice(const void* host_buffer, uint32_t size, ImageResource& image, uint32_t width, uint32_t height);

		void TransferImageToBuffer(BufferResource& buffer, uint32_t size, const ImageResource& image, uint32_t width, uint32_t height);

		void PipelineBarrier(
			VkImage image,
			VkImageLayout old_layout, VkImageLayout new_layout,
			VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
			VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
			VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT
		);

		void PipelineBarrier(
			VkBuffer buffer,
			VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask,
			VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask
		);

		void CleanUp();

		// Call this before issuing any commands.
		// Use the command buffer to do custom commands between calling Begin() and Submit().
		VkCommandBuffer& Begin();

		// Call this to submit all commands and wait for them to finish.
		void Submit();

		void NextFrame();

		FrameResources& GetCurrentFrame();

	private:
		struct QueuedHostCopy
		{
			BufferResource staging_buffer;
			void* dst;
		};

		struct FrameResources
		{
			std::vector<BufferResource> graphics_destroy_queue_{}; // Graphics staging buffers from previous frame are destroyed.
		};

		void TransferBufferToDeviceImpl(
			VkCommandBuffer cmd,
			std::vector<BufferResource>& destroy_queue,
			const void* host_buffer,
			uint32_t size,
			BufferResource& device_buffer);

		Context* context_{};
		Allocator* alloc_{};
		VkCommandPool command_pool_{};
		VkCommandBuffer cmd_{};
		std::vector<BufferResource> destroy_queue_{};   // Staging buffers are destroyed after each submit.
		std::vector<QueuedHostCopy> host_copy_queue_{}; // Staging buffers that need to be copied to the host after queue submission.
		VkFence fence_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
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
