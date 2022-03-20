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

	private:
		void CreatePipelineLayout();

		VkResult LoadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule) const;

		VkPipeline pipeline_{};
		VkPipelineLayout layout_{};
		Context* context_{};
	};
}
