#include "pipeline.h"

#include <vector>
#include <fstream>
#include "volk.h"

#include "logger.h"
#include "vulkan_util.h"

namespace renderer
{
	GraphicsPipeline::GraphicsPipeline(Context* context)
		: context_{ context }
	{
		VkShaderModule vertex_shader{};
		VkResult result{ LoadShaderModule(spirv_prefix + "default.vert.spv", &vertex_shader) };
		CheckResult(result, "Failed to create vertex shader.");

		VkShaderModule fragment_shader{};
		result = LoadShaderModule(spirv_prefix + "default.frag.spv", &fragment_shader);
		CheckResult(result, "Failed to create fragment shader.");

		VkPipelineShaderStageCreateInfo vertex_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0, // Flags are all about subgroup sizes.
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		VkPipelineShaderStageCreateInfo fragment_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		std::vector<VkPipelineShaderStageCreateInfo> stages{ vertex_stage, fragment_stage };

		VkGraphicsPipelineCreateInfo pipeline_info{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.flags = 0,
			.stageCount = stages.size(),
			.pStages = stages.data(),
			//.pVertexInputState
			//.pInputAssemblyState
			//.pTessellationState
			//.pViewportState
			//.pRasterizationState
			//.pMultisampleState
			//.pDepthStencilState
			//.pColorBlendState
			//.pDynamicState
			//.layout
			//.renderPass
			//.subpass
			//.basePipelineHandle
			//.basePipelineIndex
		};


	}

	GraphicsPipeline::~GraphicsPipeline()
	{
		vkDestroyPipelineLayout(context_->device, layout_, nullptr);
		vkDestroyPipeline(context_->device, pipeline_, nullptr);
	}

	VkResult GraphicsPipeline::LoadShaderModule(const std::string& path, VkShaderModule* out_shader_module) const
	{
		// std::ios::ate puts cursor at end of file upon opening.
		std::ifstream file{ path, std::ios::ate | std::ios::binary };

		if (!file.is_open()) {
			logger::Error("Can't open file: %s\n", path.c_str());
		}

		// Find size of file in bytes by position of cursor.
		size_t fileSize{ (size_t)file.tellg() };

		// Spirv expects the buffer to be on uint32, so make sure to
		// reserve an int vector big enough for the entire file.
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		// Put file cursor at beginning.
		file.seekg(0);

		// load the entire file into the buffer
		file.read((char*)buffer.data(), fileSize);

		file.close();

		// create a new shader module using the buffer we loaded
		VkShaderModuleCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.flags = 0,
			.codeSize = buffer.size() * sizeof(uint32_t),
			.pCode = buffer.data(),
		};

		VkShaderModule shaderModule;
		VkResult result{ vkCreateShaderModule(context_->device, &createInfo, nullptr, &shaderModule) };
		*out_shader_module = shaderModule;

		return result;
	}
}
