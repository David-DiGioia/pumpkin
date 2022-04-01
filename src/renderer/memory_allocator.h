#include <vector>
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
	};

	class Allocator
	{
	public:
		void Initialize(Context* context);

		BufferResource CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	private:
		struct Allocation
		{
			VkDeviceMemory memory;
			VkDeviceSize available_allocation_offset; // The byte offset where unbound allocated memory starts.
			VkDeviceSize available_allocated_memory; // How much allocated memory is left in this allocation?
		};

		struct MemoryTypeAllocations
		{
			VkMemoryType memory_type;
			uint32_t memory_type_index; // Needed for VkMemoryAllocateInfo.
			std::vector<Allocation> allocations;
		};

		// Returns byte offset into device memory.
		VkDeviceSize AllocateMemoryType(
			const VkMemoryRequirements& requirements,
			VkMemoryPropertyFlags properties,
			std::vector<MemoryTypeAllocations>& memory_type_allocations,
			VkDeviceMemory** out_memory
		);

		// Returns byte offset into device memory.
		VkDeviceSize AllocateMemory(
			const VkMemoryRequirements& requirements,
			VkMemoryPropertyFlags properties,
			VkDeviceMemory** out_memory
		);

		Context* context_{};
		VkPhysicalDeviceLimits limits_{};
		uint32_t remaining_allocations_{};

		std::vector<MemoryTypeAllocations> device_allocations_{};
		std::vector<MemoryTypeAllocations> host_allocations_{};
		std::vector<MemoryTypeAllocations> device_host_allocations_{};
		std::vector<VkDeviceSize> remaining_heap_memory_{};
	};
}