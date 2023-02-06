#pragma once

#include <string>
#include <filesystem>
#include "volk.h"

#include "context.h"
#include "swapchain.h"
#include "descriptor_set.h"

namespace renderer
{
	class GraphicsPipeline
	{
	public:
		// Pass VK_FORMAT_UNDEFINED for depth_format to disable depth test.
		void Initialize(
			Context* context,
			const std::vector<DescriptorSetLayoutResource>& set_layouts,
			VkFormat color_attachment_format,
			VkFormat depth_format,
			VertexAttributes attributes,
			const std::filesystem::path& vertex_shader_path,
			const std::filesystem::path& fragment_shader_path
		);

		void CleanUp();

		VkPipeline pipeline{};
		VkPipelineLayout layout{};

	private:
		void CreatePipelineLayout(const std::vector<DescriptorSetLayoutResource>& set_layouts);

		Context* context_{};
	};
}
