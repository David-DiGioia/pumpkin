#pragma once

#include <string>
#include "volk.h"

#include "context.h"

namespace renderer
{
	const std::string spirv_prefix{ "../src/renderer/shaders/spirv/" };

	class GraphicsPipeline
	{
	public:
		GraphicsPipeline(Context* context);

		~GraphicsPipeline();

	private:
		VkResult LoadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule) const;

		VkPipeline pipeline_{};
		VkPipelineLayout layout_{};
		Context* context_{};
	};
}
