#pragma once

#include <functional>
#include "volk.h"
#include "imgui.h"

namespace renderer
{
	class VulkanRenderer;

	struct EditorInfo
	{
		std::function<void(void* user_data)> initialization_callback{};
		std::function<void(ImTextureID rendered_image_id, void* user_data)> gui_callback{};
		void* user_data;
	};

	class EditorBackend
	{
	public:
		void Initialize(VulkanRenderer* renderer);

		void CleanUp();

		void RenderGui(VkCommandBuffer cmd);

		void SetEditorInfo(const EditorInfo& editor_info);

	private:
		void CreateDescriptorPool();

		void InitializeImGui();

		VulkanRenderer* renderer_{};
		EditorInfo info_{};
		VkDescriptorSet render_target_{};
		VkDescriptorPool descriptor_pool_{};
	};
}
