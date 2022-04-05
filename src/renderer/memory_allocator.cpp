#include "memory_allocator.h"

#include <algorithm>

#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	/*
	* Create three vectors of memory types, HOST_VISIBLE | DEVICE_LOCAL,
	* HOT_VISIBLE and DEVICE_LOCAL. Iterate through all types in appropriate
	* vector and choose the first one with enough remaining memory in heap that
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
			bool device_local{ (bool)(memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
			bool host_visible{ (bool)(memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };

			MemoryTypeAllocations mem_type_allocations{};
			mem_type_allocations.memory_type = memory_properties.memoryTypes[i];
			mem_type_allocations.memory_type_index = i;

			if (device_local && host_visible) {
				device_host_allocations_.push_back(mem_type_allocations);
			}
			else if (device_local) {
				device_allocations_.push_back(mem_type_allocations);
			}
			else if (host_visible) {
				host_allocations_.push_back(mem_type_allocations);
			}
		}

		remaining_heap_memory_.resize(memory_properties.memoryHeapCount);

		// Store free available memory of each heap.
		for (uint32_t i{ 0 }; i < memory_properties.memoryHeapCount; ++i) {
			remaining_heap_memory_[i] = memory_properties.memoryHeaps[i].size;
		}

		// Store physical device limits to check against later.
		VkPhysicalDeviceMaintenance3Properties maintenance_properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,
		};

		VkPhysicalDeviceProperties2 physical_device_properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &maintenance_properties,
		};

		vkGetPhysicalDeviceProperties2(context_->physical_device, &physical_device_properties);
		limits_ = physical_device_properties.properties.limits;
		remaining_allocations_ = limits_.maxMemoryAllocationCount;
		max_alloc_size_ = maintenance_properties.maxMemoryAllocationSize;
	}

	void Allocator::CleanUp()
	{
		// Free all DEVICE_LOCAL | HOST_VISIBLE allocations.
		for (MemoryTypeAllocations& mem_type_allocations : device_host_allocations_)
		{
			for (Allocation& alloc : mem_type_allocations.allocations) {
				vkFreeMemory(context_->device, alloc.memory, nullptr);
			}
		}

		// Free all DEVICE_LOCAL allocations.
		for (MemoryTypeAllocations& mem_type_allocations : device_allocations_)
		{
			for (Allocation& alloc : mem_type_allocations.allocations) {
				vkFreeMemory(context_->device, alloc.memory, nullptr);
			}
		}

		// Free all HOST_VISIBLE allocations.
		for (MemoryTypeAllocations& mem_type_allocations : host_allocations_)
		{
			for (Allocation& alloc : mem_type_allocations.allocations) {
				vkFreeMemory(context_->device, alloc.memory, nullptr);
			}
		}
	}

	BufferResource Allocator::CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		VkBufferCreateInfo buffer_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.flags = 0,
			.size = size,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr, // Ignored for sharing mode exclusive.
		};

		VkBuffer buffer{};
		VkResult result{ vkCreateBuffer(context_->device, &buffer_info, nullptr, &buffer) };
		CheckResult(result, "Failed to create buffer.");

		VkMemoryRequirements memory_requirements{};
		vkGetBufferMemoryRequirements(context_->device, buffer, &memory_requirements);

		VkDeviceMemory* memory{};
		VkDeviceSize offset{ FindMemory(memory_requirements, properties, &memory) };

		result = vkBindBufferMemory(context_->device, buffer, *memory, offset);
		CheckResult(result, "Failed to bind buffer to memory.");

		return BufferResource{
			.buffer = buffer,
			.memory = memory,
			.size = size,
			.offset = offset,
		};
	}

	// Get the lowest number we must add to offset such that it meets alignment requirement.
	VkDeviceSize GetAlignmentOffset(VkDeviceSize offset, VkDeviceSize alignment)
	{
		return (alignment - (offset % alignment)) % alignment;
	}

	VkDeviceSize Allocator::ExistingAllocation(VkDeviceSize alignment_offset, VkDeviceSize required_size, Allocation* alloc, VkDeviceMemory** out_memory)
	{
		*out_memory = &alloc->memory;

		VkDeviceSize buffer_start_offset{ alloc->available_offset + alignment_offset };

		alloc->available_offset += alignment_offset + required_size;
		alloc->available_memory -= alignment_offset + required_size;

		return buffer_start_offset;
	}

	VkDeviceSize Allocator::NewAllocation(VkDeviceSize required_size, MemoryTypeAllocations* mem_type_alloc, VkDeviceMemory** out_memory)
	{
		VkDeviceSize alloc_size{ (VkDeviceSize)(ALLOCATION_RATIO * remaining_heap_memory_[mem_type_alloc->memory_type_index]) };
		alloc_size = std::clamp(alloc_size, required_size, max_alloc_size_);

		// Otherwise make new allocation.
		VkMemoryAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = alloc_size,
			.memoryTypeIndex = mem_type_alloc->memory_type_index,
		};

		mem_type_alloc->allocations.emplace_back();
		VkResult result{ vkAllocateMemory(context_->device, &allocate_info, nullptr, &mem_type_alloc->allocations.back().memory) };
		CheckResult(result, "Failed to allocate memory.");

		// No need to worry about alignment since we are return buffer at offset 0.
		mem_type_alloc->allocations.back().available_memory = alloc_size - required_size;
		mem_type_alloc->allocations.back().available_offset = required_size;

		*out_memory = &mem_type_alloc->allocations.back().memory;
		return (VkDeviceSize)0; // Buffer will start at beginning of allocation.
	}

	VkDeviceSize Allocator::FindMemoryType(
		const VkMemoryRequirements& requirements,
		VkMemoryPropertyFlags properties,
		std::vector<MemoryTypeAllocations>& memory_type_allocations,
		VkDeviceMemory** out_memory
	)
	{
		for (MemoryTypeAllocations& memory_type_alloc : memory_type_allocations)
		{
			bool has_all_properties{ (memory_type_alloc.memory_type.propertyFlags & properties) == properties };
			bool meets_buffer_requirement{ (bool)(requirements.memoryTypeBits & (1 << memory_type_alloc.memory_type_index)) };

			if (has_all_properties && meets_buffer_requirement)
			{
				for (Allocation& alloc : memory_type_alloc.allocations)
				{
					VkDeviceSize alignment_offset{ GetAlignmentOffset(alloc.available_offset, requirements.alignment) };

					// Use existing allocation if there's enough memory left.
					if (requirements.size <= alloc.available_memory - alignment_offset) {
						return ExistingAllocation(alignment_offset, requirements.size, &alloc, out_memory);
					}
				}

				// Otherwise we need to allocate more memory of this type.
				return NewAllocation(requirements.size, &memory_type_alloc, out_memory);
			}
		}

		logger::Error("No memory type was found meeting user specified properties and physical device requirements.\n");
		return (VkDeviceSize)~0ull;
	}

	VkDeviceSize Allocator::FindMemory(
		const VkMemoryRequirements& requirements,
		VkMemoryPropertyFlags properties,
		VkDeviceMemory** out_memory
	)
	{
		bool device_local{ (bool)(properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
		bool host_visible{ (bool)(properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };

		VkDeviceSize offset = ~0ull;

		if (device_local && host_visible) {
			offset = FindMemoryType(requirements, properties, device_host_allocations_, out_memory);
		}
		else if (device_local) {
			offset = FindMemoryType(requirements, properties, device_allocations_, out_memory);
		}
		else if (host_visible) {
			offset = FindMemoryType(requirements, properties, host_allocations_, out_memory);
		}
		else {
			logger::Error("Unsupported memory properties.");
		}

		--remaining_allocations_;
		return offset;
	}
}
