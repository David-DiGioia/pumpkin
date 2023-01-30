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

	struct ImGuiCallbacks
	{
		std::function<void(void* user_data)> initialization_callback{};
		std::function<void(ImTextureID* rendered_image_id, void* user_data)> gui_callback{};
		void* user_data;
	};

	class ImGuiBackend
	{
	public:
		void Initialize(VulkanRenderer* renderer);

		void CleanUp();

		// Does not record the command buffer, only tells ImGui what it should draw later.
		void DrawGui();

		void RecordCommandBuffer(VkCommandBuffer cmd);

		void SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks);

		void SetViewportSize(const Extent& extent);

		Extent GetViewportExtent() const;

		bool GetViewportVisible() const;

		ImageResource& GetViewportImage();

		ImageResource& GetViewportDepthImage();

		std::array<ImageResource, FRAMES_IN_FLIGHT> GetRayTraceImages();

		void TransitionImagesForRender(VkCommandBuffer cmd);

		void TransitionImagesForSampling(VkCommandBuffer cmd);

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
			VkDescriptorSet render_target_descriptor;
			ImageResource render_image;
			ImageResource rt_image;
			ImageResource depth_image;
		};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};

		VulkanRenderer* renderer_{};
		ImGuiCallbacks callbacks_{};
		VkDescriptorPool descriptor_pool_{};
		Extent viewport_extent_{};
		bool viewport_visible_{};
	};

	class EditorBackend
	{
	public:
		void Initialize(VulkanRenderer* renderer);

		ImGuiBackend& GetImGuiBackend();

		void EditorRenderPass(VkCommandBuffer cmd);

	private:
		VulkanRenderer* renderer_{};
		ImGuiBackend imgui_backend_{};
	};
}
