#pragma once

#include <functional>
#include <array>
#include <unordered_set>
#include "volk.h"
#include "imgui.h"

#include "memory_allocator.h"
#include "renderer_types.h"
#include "render_object.h"
#include "pipeline.h"

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
		void Initialize(Context* context, VulkanRenderer* renderer);

		void InitializeDescriptorSetLayouts();

		ImGuiBackend& GetImGuiBackend();

		void EditorRenderPasses(VkCommandBuffer cmd, uint32_t image_index);

		void AddOutlineSet(std::vector<RenderObject*>&& selection_set, const glm::vec3& color);

		void ClearOutlineSets();

	private:
		struct OutlineObjects
		{
			std::vector<RenderObject*> render_objects;
			glm::vec3 color;
		};

		struct FrameResources
		{
			ImageResource mask_image;
			DescriptorSetResource outline_set_resource_{};
		};


		FrameResources& GetCurrentFrame();

		void InitializeFrameResources();

		void MaskRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set);

		void OutlineRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set, uint32_t image_index);

		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};

		Context* context_{};
		VulkanRenderer* renderer_{};
		ImGuiBackend imgui_backend_{};
		GraphicsPipeline mask_pipeline_{};
		GraphicsPipeline outline_pipeline_{};
		std::vector<OutlineObjects> outline_objects_{}; // Editor render pass will draw outlines around these sets of render objects.
		DescriptorSetLayoutResource outline_layout_resource_{};
	};
}
