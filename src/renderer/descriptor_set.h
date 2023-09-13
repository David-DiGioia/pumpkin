#pragma once

#include <vector>
#include <unordered_map>
#include "volk.h"

#include "context.h"
#include "memory_allocator.h"

namespace renderer
{
	struct DescriptorSetLayoutResource
	{
		VkDescriptorSetLayout layout{};
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{}; // Map of (binding, binding info).
	};

	struct DescriptorSetResource
	{
		VkDescriptorSet descriptor_set{};
		const DescriptorSetLayoutResource* layout_resource{};

		void LinkBufferToBinding(uint32_t binding, const BufferResource& buffer_resource);

		void LinkImageToBinding(uint32_t binding, const ImageResource& image_resource, VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		void LinkImageArrayToBinding(uint32_t binding, const std::vector<ImageResource*>& image_resources, VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		void LinkAccelerationStructureToBinding(uint32_t binding, VkAccelerationStructureKHR acceleration_structure);

	private:
		// We let the device secretly be stored by DescriptorSetResource to keep
		// interface of LinkBufferToBinding simple, without needing to pass device.
		VkDevice device_{};
		friend class DescriptorAllocator;
	};

	class DescriptorAllocator
	{
	public:
		void Initialize(Context* context);

		void CleanUp();

		DescriptorSetLayoutResource CreateDescriptorSetLayoutResource(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings, VkDescriptorSetLayoutCreateFlags flags);

		// Binding flags is not typically needed except for descriptor set layouts containing bindless resources.
		DescriptorSetLayoutResource CreateDescriptorSetLayoutResource(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings, const std::vector<VkDescriptorBindingFlags>& binding_flags, VkDescriptorSetLayoutCreateFlags flags);

		void DestroyDescriptorSetLayoutResource(DescriptorSetLayoutResource* layout_resource);

		DescriptorSetResource CreateDescriptorSetResource(const DescriptorSetLayoutResource& layout_resource);

		void FreeDescriptorSet(VkDescriptorSet* descriptor_set);

	private:
		Context* context_{};
		VkDescriptorPool pool_{};
	};
}
