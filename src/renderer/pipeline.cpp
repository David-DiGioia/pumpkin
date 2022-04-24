#include "pipeline.h"

#include <vector>
#include <fstream>
#include "volk.h"

#include "logger.h"
#include "vulkan_util.h"
#include "mesh.h"

namespace renderer
{
	const std::string spirv_prefix{ "../shaders/" };

	void GraphicsPipeline::Initialize(Context* context, Swapchain* swapchain, const std::vector<DescriptorSetLayoutResource>& set_layouts)
	{
		context_ = context;

		// Shaders ----------------------------------------------------------------------------

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

		// Vertex input -----------------------------------------------------------------------

		VkVertexInputBindingDescription vertex_input_binding{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		std::vector<VkVertexInputAttributeDescription> attributes{ Vertex::GetVertexAttributes() };

		VkPipelineVertexInputStateCreateInfo vertex_input_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_input_binding,
			.vertexAttributeDescriptionCount = (uint32_t)attributes.size(),
			.pVertexAttributeDescriptions = attributes.data(),
		};

		VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineTessellationStateCreateInfo tesselation_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.patchControlPoints = 1,
		};

		VkPipelineViewportStateCreateInfo viewport_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.viewportCount = 1,
			.pViewports = nullptr, // Ignored for dynamic viewport.
			.scissorCount = 1,
			.pScissors = nullptr, // Ignored for dynamic scissor.
		};

		VkPipelineRasterizationStateCreateInfo rasterization_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisample_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 1.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depth_stencil_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.flags = 0, // Only extension flags.
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		VkPipelineColorBlendAttachmentState color_blend_attachment{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo color_blend_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.flags = 0, // Only extension flags.
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &color_blend_attachment,
			.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
		};

		std::vector<VkDynamicState> dynamic_states{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamic_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.flags = 0,
			.dynamicStateCount = (uint32_t)dynamic_states.size(),
			.pDynamicStates = dynamic_states.data(),
		};

		CreatePipelineLayout(set_layouts);

		VkFormat swapchain_image_format = swapchain->GetImageFormat();

		// Dynamic rendering.
		VkPipelineRenderingCreateInfo rendering_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain_image_format,
			.depthAttachmentFormat = {}, // TODO.
			.stencilAttachmentFormat = {}, // TODO.
		};

		VkGraphicsPipelineCreateInfo pipeline_info{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &rendering_info,
			.flags = 0,
			.stageCount = (uint32_t)stages.size(),
			.pStages = stages.data(),
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_info,
			.pTessellationState = &tesselation_info,
			.pViewportState = &viewport_info,
			.pRasterizationState = &rasterization_info,
			.pMultisampleState = &multisample_info,
			.pDepthStencilState = nullptr, // No depth buffer yet.
			.pColorBlendState = &color_blend_info,
			.pDynamicState = &dynamic_info,
			.layout = layout,
			.renderPass = VK_NULL_HANDLE, // We're using dynamic rendering.
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0,
		};

		result = vkCreateGraphicsPipelines(context_->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
		CheckResult(result, "Failed to create graphics pipeline.");

		vkDestroyShaderModule(context_->device, vertex_shader, nullptr);
		vkDestroyShaderModule(context_->device, fragment_shader, nullptr);
	}

	void GraphicsPipeline::CleanUp()
	{
		vkDestroyPipelineLayout(context_->device, layout, nullptr);
		vkDestroyPipeline(context_->device, pipeline, nullptr);
	}

	void GraphicsPipeline::CreatePipelineLayout(const std::vector<DescriptorSetLayoutResource>& set_layouts)
	{
		// Convert layout resources to Vulkan set layouts.
		std::vector<VkDescriptorSetLayout> vk_set_layouts;
		vk_set_layouts.reserve(set_layouts.size());
		for (auto& set_layout : set_layouts) {
			vk_set_layouts.push_back(set_layout.layout);
		}

		VkPipelineLayoutCreateInfo layout_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.flags = 0,
			.setLayoutCount = (uint32_t)vk_set_layouts.size(),
			.pSetLayouts = vk_set_layouts.data(),
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};

		VkResult result{ vkCreatePipelineLayout(context_->device, &layout_info, nullptr, &layout) };
		CheckResult(result, "Failed to create pipeline layout.");
	}

	VkResult GraphicsPipeline::LoadShaderModule(const std::string& path, VkShaderModule* out_shader_module) const
	{
		// std::ios::ate puts cursor at end of file upon opening.
		std::ifstream file{ path, std::ios::ate | std::ios::binary };

		if (!file.is_open()) {
			logger::Error("Can't open file: %s\n", path.c_str());
		}

		// Find size of file in bytes by position of cursor.
		size_t file_size{ (size_t)file.tellg() };

		// Spirv expects the buffer to be on uint32, so make sure to
		// reserve an int vector big enough for the entire file.
		std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

		// Put file cursor at beginning.
		file.seekg(0);

		// load the entire file into the buffer
		file.read((char*)buffer.data(), file_size);

		file.close();

		// create a new shader module using the buffer we loaded
		VkShaderModuleCreateInfo module_info{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.flags = 0,
			.codeSize = buffer.size() * sizeof(uint32_t),
			.pCode = buffer.data(),
		};

		VkShaderModule shader_module;
		VkResult result{ vkCreateShaderModule(context_->device, &module_info, nullptr, &shader_module) };
		*out_shader_module = shader_module;

		return result;
	}
}
