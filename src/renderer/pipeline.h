#pragma once

#include <string>
#include "volk.h"

#include "context.h"
#include "swapchain.h"
#include "descriptor_set.h"

namespace renderer
{
	class GraphicsPipeline
	{
	public:
		void Initialize(Context* context, Swapchain* swapchain, const std::vector<DescriptorSetLayoutResource>& set_layouts, VkFormat depth_format);

		void CleanUp();

		VkPipeline pipeline{};
		VkPipelineLayout layout{};

	private:
		void CreatePipelineLayout(const std::vector<DescriptorSetLayoutResource>& set_layouts);

		VkResult LoadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule) const;

		Context* context_{};
	};
}
