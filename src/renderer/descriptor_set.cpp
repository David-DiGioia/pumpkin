#include "descriptor_set.h"

#include <vector>

#include "vulkan_util.h"

namespace renderer
{
	void DescriptorSetResource::LinkBufferToBinding(uint32_t binding, const BufferResource& buffer_resource)
	{
		VkDescriptorBufferInfo buffer_info{
			.buffer = buffer_resource.buffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};

		VkWriteDescriptorSet write_info{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = layout_resource->bindings.at(binding).descriptorType,
			.pImageInfo = nullptr,
			.pBufferInfo = &buffer_info,
			.pTexelBufferView = nullptr,
		};

		vkUpdateDescriptorSets(device_, 1, &write_info, 0, nullptr);
	}

	void DescriptorSetResource::LinkImageToBinding(uint32_t binding, const ImageResource& image_resource, VkImageLayout image_layout)
	{
		// Const cast because LinkImageArrayToBinding will not modify image_resource, but it needs to take a non-const ImageResource*.
		LinkImageArrayToBinding(binding, { &const_cast<ImageResource&>(image_resource) }, image_layout);
	}

	void DescriptorSetResource::LinkImageArrayToBinding(uint32_t binding, const std::vector<ImageResource*>& image_resources, VkImageLayout image_layout)
	{
		std::vector<VkWriteDescriptorSet> write_infos{};
		write_infos.reserve(image_resources.size());

		uint32_t i{ 0 };
		for (const ImageResource* image_resource : image_resources)
		{
			// Heap allocate so lifetime persists outside of loop.
			VkDescriptorImageInfo* image_info{ new VkDescriptorImageInfo{
					.sampler = image_resource->sampler,
					.imageView = image_resource->image_view,
					.imageLayout = image_layout,
				}
			};

			VkWriteDescriptorSet write_info{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptor_set,
				.dstBinding = binding,
				.dstArrayElement = i,
				.descriptorCount = 1,
				.descriptorType = layout_resource->bindings.at(binding).descriptorType,
				.pImageInfo = image_info,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			};

			write_infos.push_back(write_info);

			++i;
		}

		vkUpdateDescriptorSets(device_, (uint32_t)write_infos.size(), write_infos.data(), 0, nullptr);

		// Clean up descriptor image infos.
		for (VkWriteDescriptorSet& write_info : write_infos) {
			delete write_info.pImageInfo;
		}
	}

	void DescriptorSetResource::LinkAccelerationStructureToBinding(uint32_t binding, VkAccelerationStructureKHR acceleration_structure)
	{
		VkWriteDescriptorSetAccelerationStructureKHR write_descriptor_acceleration_structure{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			.accelerationStructureCount = 1,
			.pAccelerationStructures = &acceleration_structure,
		};

		VkWriteDescriptorSet write_info{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = &write_descriptor_acceleration_structure,
			.dstSet = descriptor_set,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = layout_resource->bindings.at(binding).descriptorType,
			.pImageInfo = nullptr,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		};

		vkUpdateDescriptorSets(device_, 1, &write_info, 0, nullptr);
	}

	void DescriptorAllocator::Initialize(Context* context)
	{
		context_ = context;

		constexpr uint32_t max_descriptor_sets{ 100 };
		constexpr uint32_t base_pool_size_count{ 1000 };

		std::vector<VkDescriptorPoolSize> pool_sizes{
			{ VK_DESCRIPTOR_TYPE_SAMPLER,                (uint32_t)(0.5f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)(4.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          (uint32_t)(4.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          (uint32_t)(1.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   (uint32_t)(1.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   (uint32_t)(1.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         (uint32_t)(2.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         (uint32_t)(2.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, (uint32_t)(1.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, (uint32_t)(1.0f * base_pool_size_count) },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       (uint32_t)(0.5f * base_pool_size_count) }
		};

		VkDescriptorPoolCreateInfo pool_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets = max_descriptor_sets,
			.poolSizeCount = (uint32_t)pool_sizes.size(),
			.pPoolSizes = pool_sizes.data(),
		};

		VkResult result{ vkCreateDescriptorPool(context_->device, &pool_info, nullptr, &pool_) };
		CheckResult(result, "Failed to create descriptor pool.");
		NameObject(context->device, pool_, "Main_Descriptor_Pool");
	}

	void DescriptorAllocator::CleanUp()
	{
		vkDestroyDescriptorPool(context_->device, pool_, nullptr);
	}

	DescriptorSetLayoutResource DescriptorAllocator::CreateDescriptorSetLayoutResource(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings, VkDescriptorSetLayoutCreateFlags flags)
	{
		return CreateDescriptorSetLayoutResource(layout_bindings, {}, flags);
	}

	DescriptorSetLayoutResource DescriptorAllocator::CreateDescriptorSetLayoutResource(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings, const std::vector<VkDescriptorBindingFlags>& binding_flags, VkDescriptorSetLayoutCreateFlags flags)
	{
		DescriptorSetLayoutResource layout_resource{};

		// Convert vector to unordered map.
		for (auto& binding_info : layout_bindings) {
			layout_resource.bindings[binding_info.binding] = binding_info;
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = (uint32_t)binding_flags.size(),
			.pBindingFlags = binding_flags.data(),
		};

		VkDescriptorSetLayoutCreateInfo descriptor_layout_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = binding_flags.empty() ? nullptr : &binding_flags_info,
			.flags = flags,
			.bindingCount = (uint32_t)layout_bindings.size(),
			.pBindings = layout_bindings.data(),
		};

		VkResult result{ vkCreateDescriptorSetLayout(context_->device, &descriptor_layout_info, nullptr, &layout_resource.layout) };
		CheckResult(result, "Failed to create descriptor set layout.");

		return layout_resource;
	}

	void DescriptorAllocator::DestroyDescriptorSetLayoutResource(DescriptorSetLayoutResource* layout_resource)
	{
		vkDestroyDescriptorSetLayout(context_->device, layout_resource->layout, nullptr);
	}

	DescriptorSetResource DescriptorAllocator::CreateDescriptorSetResource(const DescriptorSetLayoutResource& layout_resource)
	{
		DescriptorSetResource resource{};
		resource.layout_resource = &layout_resource;
		resource.device_ = context_->device;

		// Allocate descriptor set.
		VkDescriptorSetAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = pool_,
			.descriptorSetCount = 1,
			.pSetLayouts = &layout_resource.layout,
		};

		VkResult result{ vkAllocateDescriptorSets(context_->device, &allocate_info, &resource.descriptor_set) };
		CheckResult(result, "Failed to allocate descriptor sets.");

		return resource;
	}
}

