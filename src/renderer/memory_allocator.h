#pragma once

#include <vector>
#include <unordered_map>
#include <set>
#include "volk.h"

#include "context.h"

namespace renderer
{
	struct BufferResource
	{
		VkBuffer buffer;
		VkDeviceMemory* memory; // Should be shared by multiple buffers.
		VkDeviceSize size; // Byte size of buffer.
		VkDeviceSize offset; // Offset into memory where the buffer starts.

		const BufferResource& operator=(const BufferResource& other);
	};

	struct ImageResource
	{
		VkImage image;
		VkImageView image_view;
		VkSampler sampler;
		VkExtent2D extent;
		VkFormat format;
		VkDeviceMemory* memory; // Should be shared by multiple buffers.
		VkDeviceSize offset; // Offset into memory where the image starts.

		const ImageResource& operator=(const ImageResource& other);
	};

	class Allocator
	{
	public:
		void Initialize(Context* context);

		void CleanUp();

		BufferResource CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

		ImageResource CreateImageResource(
			Extent extent,
			VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties,
			VkFormat format
		);

		void DestroyBufferResource(BufferResource* buffer_resource);

		void DestroyImageResource(ImageResource* image_resource);

	private:
		struct Allocation
		{
			VkDeviceMemory memory;
			VkDeviceSize available_offset; // The byte offset where unbound allocated memory starts.
			VkDeviceSize available_memory; // How much allocated memory is left in this allocation?
			std::set<VkDeviceSize> buffer_offsets; // Offsets of each buffer bound to this allocation.
		};

		// Associated to each bound buffer via 
		struct BufferAllocationInfo
		{
			Allocation* allocation;
			VkDeviceSize next_available_offset;
		};

		struct MemoryTypeAllocations
		{
			VkMemoryType memory_type;
			uint32_t memory_type_index; // Needed for VkMemoryAllocateInfo.
			std::vector<Allocation> allocations;
		};

		// Get info of an existing allocation for buffer to be bound to.
		//
		// Returns byte offset into device memory.
		VkDeviceSize ExistingAllocation(
			uint64_t resource_handle,
			VkDeviceSize alignment_offset,
			VkDeviceSize required_size,
			Allocation* alloc,
			VkDeviceMemory** out_memory
		);

		// Make a new memory allocation.
		// 
		// Returns byte offset into device memory.
		VkDeviceSize NewAllocation(
			uint64_t resource_handle,
			VkDeviceSize required_size,
			MemoryTypeAllocations* alloc,
			VkDeviceMemory** out_memory
		);

		// Find an allocation of a specific memory type to bind a buffer to. If an allocation
		// cannot be found, make a new allocation.
		// 
		// Returns byte offset into device memory.
		VkDeviceSize FindMemoryType(
			uint64_t resource_handle,
			const VkMemoryRequirements& requirements,
			VkMemoryPropertyFlags properties,
			std::vector<MemoryTypeAllocations>& memory_type_allocations,
			VkDeviceMemory** out_memory
		);

		// Find an allocation to bind a buffer to. If an allocation cannot be found,
		// make a new allocation.
		// 
		// Returns byte offset into device memory.
		VkDeviceSize FindMemory(
			uint64_t resource_handle,
			const VkMemoryRequirements& requirements,
			VkMemoryPropertyFlags properties,
			VkDeviceMemory** out_memory
		);

		// Update the offsets associated with an allocation after a resource has been destroyed.
		// This includes reclaiming memory that is no longer bound to a buffer/image.
		void UpdateAllocationOffsets(uint64_t resource_handle);

		Context* context_{};
		VkPhysicalDeviceLimits limits_{};
		uint32_t remaining_allocations_{};
		VkDeviceSize max_alloc_size_{};

		std::vector<MemoryTypeAllocations> device_allocations_{};
		std::vector<MemoryTypeAllocations> host_allocations_{};
		std::vector<MemoryTypeAllocations> device_host_allocations_{};
		std::vector<VkDeviceSize> remaining_heap_memory_{}; // Remaining memory corresponding to each memory heap in VkPhysicalDeviceMemoryProperties::memoryHeaps.
		std::unordered_map<uint64_t, BufferAllocationInfo> allocation_info_map_{}; // Enables mapping buffer resources to their allocations. The key is a Vulkan handle cast to uint64_t.
	};
}