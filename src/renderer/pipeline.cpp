#include "pipeline.h"

#include "volk.h"

GraphicsPipeline::GraphicsPipeline()
{
	VkPipelineShaderStageCreateInfo vertex_stage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.flags = 0, // Flags are all about subgroup sizes.
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		//.module
		//.pName
		.pSpecializationInfo = nullptr,
	};

	VkGraphicsPipelineCreateInfo pipeline_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.flags = 0,
		//.stageCount
		//.pStages
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

}

