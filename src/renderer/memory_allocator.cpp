#include "memory_allocator.h"

#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	/*
	* Possible implementation for allocator:
	* Create three vectors of memory types, HOST_VISIBLE | DEVICE_LOCAL,
	* HOT_VISIBLE and DEVICE_LOCAL. Iterate through all types in appropriate
	* vector and choose the one with the largest remaining memory heap that
	* satisfies all the usage flags passed to CreateBufferResource.
	*
	* Allocate some percent, eg 10% of remaining memory from the chosen heap,
	* if there is not already an allocation for this memory type.
	*
	* Otherwise bind the buffer to the next available slot of the allocated
	* memory, following alignment restrictions.
	*
	* Provide a MemoryAudit() function which checks if any memory type has passed
	* some threshold of using its allocated memory. For example, if more than 90%
	* of the allocated memory for a memory type has been used, reallocate that
	* memory type with a bigger allocation. Maybe an additional 10% of available
	* memory heap for that memory type.
	*
	* When buffers are rebound, remove all fragmentation that existed before.
	*
	* Return true if a MemoryRestructure() was triggered and false otherwise.
	*
	* This operation could be costly and cause stalls. So it should be called by
	* the application only when a stall would be acceptable.
	*
	* Should make a separate function to request an optional HOST_VISIBLE | DEVICE_LOCAL
	* buffer, is a single struct containing a staging buffer and device local buffer.
	* If machine has resizable bar, the staging buffer is unused, and only device local
	* buffer is used, otherwise the staging buffer get's transferred directly before the
	* draw call. This should be opaque to the user. Also have separate Draw() and DrawMutable()
	* functions, where DrawMutable() allows writing to ssbo in shader, and therefore will
	* copy the buffer from GPU to CPU after the draw.
	*/

	// Allocate this ratio of available memory in a heap.
	constexpr float ALLOCATION_RATIO{ 0.10f };

	void Allocator::Initialize(Context* context)
	{
		context_ = context;

		VkPhysicalDeviceMemoryProperties memory_properties{};
		vkGetPhysicalDeviceMemoryProperties(context_->physical_device, &memory_properties);

		// Separate memory types into categories.
		// So if only DEVICE_LOCAL is specified, it will be not be HOST_VISIBLE too, for example.
		for (uint32_t i{ 0 }; i < memory_properties.memoryTypeCount; ++i)
		{
			bool device_local{ memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
			bool host_visible{ memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT };

			MemoryTypeAllocations allocation{};
			allocation.memory_type = memory_properties.memoryTypes[i];
			allocation.memory_type_index = i;

			if (device_local && host_visible) {
				device_host_allocations_.push_back(allocation);
			}
			else if (device_local) {
				device_allocations_.push_back(allocation);
			}
			else if (host_visible) {
				host_allocations_.push_back(allocation);
			}
		}

		// Store free available memory of each heap.
		for (uint32_t i{ 0 }; i < memory_properties.memoryHeapCount; ++i) {
			remaining_heap_memory_[i] = memory_properties.memoryHeaps[i].size;
		}

		// Store physical device limits to check against later.
		VkPhysicalDeviceProperties physical_device_properties{};
		vkGetPhysicalDeviceProperties(context_->physical_device, &physical_device_properties);
		limits_ = physical_device_properties.limits;
		remaining_allocations_ = limits_.maxMemoryAllocationCount;
	}

	BufferResource Allocator::CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		BufferResource resource{};
		resource.size = size;

		VkBufferCreateInfo buffer_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.flags = 0,
			.size = size,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr, // Ignored for sharing mode exclusive.
		};

		VkResult result{ vkCreateBuffer(context_->device, &buffer_info, nullptr, &resource.buffer) };
		CheckResult(result, "Failed to create buffer.");

		VkMemoryRequirements memory_requirements{};
		vkGetBufferMemoryRequirements(context_->device, resource.buffer, &memory_requirements);

		resource.offset = AllocateMemory(memory_requirements, properties, &resource.memory);
	}

	// Get the lowest number we must add to offset such that it meets alignment requirement.
	VkDeviceSize GetAlignmentOffset(VkDeviceSize offset, VkDeviceSize alignment)
	{
		return (alignment - (offset % alignment)) % alignment;
	}

	VkDeviceSize Allocator::AllocateMemoryType(
		const VkMemoryRequirements& requirements,
		VkMemoryPropertyFlags properties,
		std::vector<MemoryTypeAllocations>& memory_type_allocations,
		VkDeviceMemory** out_memory
	)
	{
		for (MemoryTypeAllocations& memory_type_alloc : memory_type_allocations)
		{
			bool has_all_properties{ (memory_type_alloc.memory_type.propertyFlags & properties) == properties };
			bool meets_buffer_requirement{ requirements.memoryTypeBits & (1 << memory_type_alloc.memory_type_index) };

			if (has_all_properties && meets_buffer_requirement)
			{
				for (Allocation& alloc : memory_type_alloc.allocations)
				{
					VkDeviceSize alignment_offset{ GetAlignmentOffset(alloc.available_offset, requirements.alignment)};

					// Use existing allocation if there's enough memory left.
					if (alloc.available_memory - alignment_offset >= requirements.size)
					{
						*out_memory = &alloc.memory;

						VkDeviceSize buffer_start_offset{ alloc.available_offset + alignment_offset };

						alloc.available_offset += alignment_offset + requirements.size;
						alloc.available_memory -= alignment_offset + requirements.size;

						return buffer_start_offset;
					}
				}

				// Otherwise make new allocation.
				VkMemoryAllocateInfo allocate_info{
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = requirements.size,
					.memoryTypeIndex = memory_type_alloc.memory_type_index,
				};

				// TODO: pickup here.

				memory_type_alloc.allocations.emplace_back();
				vkAllocateMemory(context_->device, &allocate_info, nullptr, &memory_type_alloc.allocations.back().memory);
			}
		}
	}

	VkDeviceSize Allocator::AllocateMemory(
		const VkMemoryRequirements& requirements,
		VkMemoryPropertyFlags properties,
		VkDeviceMemory** out_memory
	)
	{
		bool device_local{ properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
		bool host_visible{ properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT };

		VkDeviceSize offset = ~0ull;

		if (device_local && host_visible) {
			offset = AllocateMemoryType(requirements, properties, device_host_allocations_, out_memory);
		}
		else if (device_local) {
			offset = AllocateMemoryType(requirements, properties, device_allocations_, out_memory);
		}
		else if (host_visible) {
			offset = AllocateMemoryType(requirements, properties, host_allocations_, out_memory);
		}
		else {
			logger::Error("Unsupported memory properties.");
		}

		--remaining_allocations_;
		return offset;
	}
}
