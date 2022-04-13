#pragma once

#include <string>
#include "volk.h"

#include "context.h"
#include "swapchain.h"

namespace renderer
{
	class GraphicsPipeline
	{
	public:
		void Initialize(Context* context, Swapchain* swapchain);

		void CleanUp();

		VkPipeline pipeline{};
		VkPipelineLayout layout{};

	private:
		void CreatePipelineLayout();

		VkResult LoadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule) const;

		Context* context_{};
	};
}
