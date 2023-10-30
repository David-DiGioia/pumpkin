#include "pipeline.h"

#include <vector>
#include <algorithm>
#include "volk.h"

#include "logger.h"
#include "vulkan_util.h"
#include "mesh.h"
#include "renderer_constants.h"

namespace renderer
{
	void CreatePipelineLayout(
		VkDevice device,
		const std::vector<DescriptorSetLayoutResource>& set_layouts,
		const std::vector<VkPushConstantRange>& push_constant_ranges,
		VkPipelineLayout* out_layout)
	{
		// Convert layout resources to Vulkan set layouts.
		std::vector<VkDescriptorSetLayout> vk_set_layouts(set_layouts.size());
		std::transform(set_layouts.begin(), set_layouts.end(), vk_set_layouts.begin(), [](const DescriptorSetLayoutResource& layout_resource) {
			return layout_resource.layout;
			});

		VkPipelineLayoutCreateInfo layout_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.flags = 0,
			.setLayoutCount = (uint32_t)vk_set_layouts.size(),
			.pSetLayouts = vk_set_layouts.data(),
			.pushConstantRangeCount = (uint32_t)push_constant_ranges.size(),
			.pPushConstantRanges = push_constant_ranges.data(),
		};

		VkResult result{ vkCreatePipelineLayout(device, &layout_info, nullptr, out_layout) };
		CheckResult(result, "Failed to create pipeline layout.");
	}

	void GraphicsPipeline::Initialize(
		Context* context,
		const std::vector<DescriptorSetLayoutResource>& set_layouts,
		const std::vector<VkPushConstantRange>& push_constant_ranges,
		VkFormat color_attachment_format,
		VkFormat depth_format,
		VertexAttributes attributes,
		VkPrimitiveTopology topology,
		const std::filesystem::path& vertex_shader_path,
		const std::filesystem::path& fragment_shader_path)
	{
		context_ = context;

		// Shaders ----------------------------------------------------------------------------

		VkShaderModule vertex_shader{ LoadShaderModule(context_->device, vertex_shader_path) };
		VkShaderModule fragment_shader{ LoadShaderModule(context_->device, fragment_shader_path) };

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

		std::vector<VkVertexInputBindingDescription> vertex_input_bindings{};
		std::vector<VkVertexInputAttributeDescription> vertex_attributes{};

		switch (attributes)
		{
		case VertexAttributes::MPM_PARTICLE:
		{
			VkVertexInputBindingDescription vertex_input_binding{
				.binding = VERTEX_BINDING,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};

			VkVertexInputBindingDescription instance_input_binding{
				.binding = INSTANCE_BINDING,
				.stride = sizeof(MPMDebugParticleInstance),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
			};

			vertex_input_bindings = { vertex_input_binding, instance_input_binding };
			vertex_attributes = MPMDebugParticleInstance::GetVertexAttributes();
			break;
		}
		case VertexAttributes::MPM_NODE:
		{
			VkVertexInputBindingDescription vertex_input_binding{
				.binding = VERTEX_BINDING,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};

			VkVertexInputBindingDescription instance_input_binding{
				.binding = INSTANCE_BINDING,
				.stride = sizeof(MPMDebugNodeInstance),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
			};

			vertex_input_bindings = { vertex_input_binding, instance_input_binding };
			vertex_attributes = MPMDebugNodeInstance::GetVertexAttributes();
			break;
		}
		default:
		{
			VkVertexInputBindingDescription vertex_input_binding{
				.binding = VERTEX_BINDING,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};

			vertex_input_bindings = { vertex_input_binding };
			vertex_attributes = Vertex::GetVertexAttributes(attributes);
		}
		}

		VkPipelineVertexInputStateCreateInfo vertex_input_info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

		if (attributes != VertexAttributes::NONE)
		{
			vertex_input_info = VkPipelineVertexInputStateCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.flags = 0, // Reserved.
				.vertexBindingDescriptionCount = (uint32_t)vertex_input_bindings.size(),
				.pVertexBindingDescriptions = vertex_input_bindings.data(),
				.vertexAttributeDescriptionCount = (uint32_t)vertex_attributes.size(),
				.pVertexAttributeDescriptions = vertex_attributes.data(),
			};
		}

		VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.flags = 0, // Reserved.
			.topology = topology,
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
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
			.depthTestEnable = depth_format == VK_FORMAT_UNDEFINED ? VK_FALSE : VK_TRUE,
			.depthWriteEnable = depth_format == VK_FORMAT_UNDEFINED ? VK_FALSE : VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {}, // For stencil test.
			.back = {},  // For stencil test.
			.minDepthBounds = 0.0f, // For bounds test.
			.maxDepthBounds = 1.0f, // For bounds test.
		};

		VkPipelineColorBlendAttachmentState color_blend_attachment{
			.blendEnable = VK_FALSE,
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

		CreatePipelineLayout(context_->device, set_layouts, push_constant_ranges, &layout);

		// Dynamic rendering.
		VkPipelineRenderingCreateInfo rendering_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &color_attachment_format,
			.depthAttachmentFormat = depth_format,
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
			.pDepthStencilState = &depth_stencil_info,
			.pColorBlendState = &color_blend_info,
			.pDynamicState = &dynamic_info,
			.layout = layout,
			.renderPass = VK_NULL_HANDLE, // We're using dynamic rendering.
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0,
		};

		VkResult result{ vkCreateGraphicsPipelines(context_->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) };
		CheckResult(result, "Failed to create graphics pipeline.");

		vkDestroyShaderModule(context_->device, vertex_shader, nullptr);
		vkDestroyShaderModule(context_->device, fragment_shader, nullptr);
	}

	void GraphicsPipeline::CleanUp()
	{
		vkDestroyPipelineLayout(context_->device, layout, nullptr);
		vkDestroyPipeline(context_->device, pipeline, nullptr);
	}

	void ComputePipeline::Initialize(Context* context, const std::vector<DescriptorSetLayoutResource>& set_layouts, const std::vector<VkPushConstantRange>& push_constant_ranges, const std::filesystem::path& shader_path)
	{
		context_ = context;

		VkShaderModule shader{ LoadShaderModule(context_->device, shader_path) };

		VkPipelineShaderStageCreateInfo shader_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0, // Flags are all about subgroup sizes.
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = shader,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		CreatePipelineLayout(context_->device, set_layouts, push_constant_ranges, &layout);

		VkComputePipelineCreateInfo pipeline_info{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.flags = 0,
			.stage = shader_stage,
			.layout = layout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		VkResult result{ vkCreateComputePipelines(context_->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) };
		CheckResult(result, "Failed to create compute pipeline.");

		vkDestroyShaderModule(context_->device, shader, nullptr);
	}

	void ComputePipeline::CleanUp()
	{
		vkDestroyPipelineLayout(context_->device, layout, nullptr);
		vkDestroyPipeline(context_->device, pipeline, nullptr);
	}
}
