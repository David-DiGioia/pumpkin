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
			.flags = 0,
			.maxSets = max_descriptor_sets,
			.poolSizeCount = (uint32_t)pool_sizes.size(),
			.pPoolSizes = pool_sizes.data(),
		};

		VkResult result{ vkCreateDescriptorPool(context_->device, &pool_info, nullptr, &pool_) };
		CheckResult(result, "Failed to create descriptor pool.");
	}

	void DescriptorAllocator::CleanUp()
	{
		vkDestroyDescriptorPool(context_->device, pool_, nullptr);
	}

	DescriptorSetLayoutResource DescriptorAllocator::CreateLayoutResource(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings)
	{
		DescriptorSetLayoutResource layout_resource{};

		// Convert vector to unordered map.
		for (auto& binding_info : layout_bindings) {
			layout_resource.bindings[binding_info.binding] = binding_info;
		}

		VkDescriptorSetLayoutCreateInfo descriptor_layout_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = 0,
			.bindingCount = (uint32_t)layout_bindings.size(),
			.pBindings = layout_bindings.data(),
		};

		VkResult result{ vkCreateDescriptorSetLayout(context_->device, &descriptor_layout_info, nullptr, &layout_resource.layout) };
		CheckResult(result, "Failed to create descriptor set layout.");

		return layout_resource;
	}


	DescriptorSetResource DescriptorAllocator::CreateDescriptorSetResource(const DescriptorSetLayoutResource& layout_resource)
	{
		//VkDescriptorSetLayoutBinding ubo_binding{
		//	.binding = 0,
		//	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		//	.descriptorCount = 1,
		//	.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		//	.pImmutableSamplers = nullptr,
		//};

		//std::vector<VkDescriptorSetLayoutBinding> bindings{
		//	ubo_binding,
		//};

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

