#pragma once

#include <functional>
#include <array>
#include "volk.h"
#include "imgui.h"

#include "memory_allocator.h"
#include "renderer_types.h"

namespace renderer
{
	class VulkanRenderer;

	struct EditorInfo
	{
		std::function<void(void* user_data)> initialization_callback{};
		std::function<void(ImTextureID* rendered_image_id, void* user_data)> gui_callback{};
		void* user_data;
	};

	class EditorBackend
	{
	public:
		void Initialize(VulkanRenderer* renderer);

		void CleanUp();

		// Does not record the command buffer, only tells ImGui what it should draw later.
		void DrawGui();

		void RecordCommandBuffer(VkCommandBuffer cmd);

		void SetEditorInfo(const EditorInfo& editor_info);

		void SetViewportSize(const Extent& extent);

		Extent GetViewportExtent() const;

		ImageResource& GetViewportImage();

	private:
		struct FrameResources;

		FrameResources& GetCurrentFrame();

		void CreateFrameResources(Extent extent);

		void DestroyFrameResources();

		void CreateDescriptorPool();

		void InitializeImGui();

		struct FrameResources
		{
			// We use raw descriptor set here instead of resource since ImGui creates it for us.
			VkDescriptorSet render_target_descriptor_{};
			ImageResource render_image_{};
		};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};

		VulkanRenderer* renderer_{};
		EditorInfo info_{};
		VkDescriptorPool descriptor_pool_{};
		Extent viewport_extent_{};
	};
}
