#include "editor_backend.h"

#include <vector>
#include "volk.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "implot/implot.h"

#include "vulkan_renderer.h"
#include "vulkan_util.h"

namespace renderer
{
	void EditorBackend::Initialize(VulkanRenderer* renderer)
	{
		renderer_ = renderer;

		CreateDescriptorPool();
		InitializeImGui();

		// Call editor's custom initialization function.
		info_.initialization_callback(info_.user_data);
	}

	void EditorBackend::CleanUp()
	{
		DestroyFrameResources();
		vkDestroyDescriptorPool(renderer_->context_.device, descriptor_pool_, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void EditorBackend::DrawGui()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Call editor's custom gui callback.
		// The callback will update the viewport extent here if it needs to.
		info_.gui_callback((ImTextureID*)&GetCurrentFrame().render_target_descriptor_, info_.user_data);

		ImGui::Render();
	}

	void EditorBackend::RecordCommandBuffer(VkCommandBuffer cmd)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	void EditorBackend::SetEditorInfo(const EditorInfo& editor_info)
	{
		info_ = editor_info;
	}

	void EditorBackend::SetViewportSize(const Extent& extent)
	{
		if (extent != viewport_extent_)
		{
			viewport_extent_ = extent;
			viewport_visible_ = (extent.width != 0) && (extent.height != 0);

			if (viewport_visible_)
			{
				DestroyFrameResources();
				CreateFrameResources(extent);
			}
		}
	}

	Extent EditorBackend::GetViewportExtent() const
	{
		return viewport_extent_;
	}

	ImageResource& EditorBackend::GetViewportImage()
	{
		return GetCurrentFrame().render_image_;
	}

	bool EditorBackend::GetViewportVisible() const
	{
		return viewport_visible_;
	}

	EditorBackend::FrameResources& EditorBackend::GetCurrentFrame()
	{
		return frame_resources_[renderer_->current_frame_];
	}

	void EditorBackend::CreateFrameResources(Extent extent)
	{
		for (FrameResources& resource : frame_resources_)
		{
			resource.render_image_ = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Color attachment to render into, then ImGui samples from it.
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				renderer_->swapchain_.GetImageFormat()
			);

			resource.render_target_descriptor_ = ImGui_ImplVulkan_AddTexture(
				resource.render_image_.sampler,
				resource.render_image_.image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
		}
	}

	void EditorBackend::DestroyFrameResources()
	{
		VkResult result{ vkDeviceWaitIdle(renderer_->context_.device) };
		CheckResult(result, "Error waiting for device to idle.");

		for (FrameResources& resource : frame_resources_)
		{
			renderer_->allocator_.DestroyImageResource(&resource.render_image_);

			result = vkFreeDescriptorSets(renderer_->context_.device, descriptor_pool_, 1, &resource.render_target_descriptor_);
			CheckResult(result, "Failed to free descriptor set.");
			resource.render_target_descriptor_ = VK_NULL_HANDLE;
		}
	}

	void EditorBackend::CreateDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> pool_sizes{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
		};

		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 1000,
			.poolSizeCount = (uint32_t)pool_sizes.size(),
			.pPoolSizes = pool_sizes.data(),
		};

		VkResult result{ vkCreateDescriptorPool(renderer_->context_.device, &pool_info, nullptr, &descriptor_pool_) };
		CheckResult(result, "Failed to create imgui descriptor pool.");
	}

	void EditorBackend::InitializeImGui()
	{
		// This initializes the core structures of imgui
		ImGui::CreateContext();
		ImPlot::CreateContext();

		// This initializes imgui for GLFW.
		ImGui_ImplGlfw_InitForVulkan(renderer_->context_.window, true);

		// This initializes imgui for Vulkan.
		ImGui_ImplVulkan_InitInfo init_info{
			.Instance = renderer_->context_.instance,
			.PhysicalDevice = renderer_->context_.physical_device,
			.Device = renderer_->context_.device,
			.Queue = renderer_->context_.graphics_queue,
			.DescriptorPool = descriptor_pool_,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.UseDynamicRendering = true,
			.ColorAttachmentFormat = renderer_->swapchain_.GetImageFormat(),
		};

		// Pass null handle since we're using dynamic rendering.
		ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

		// Execute a gpu command to upload imgui font textures.
		auto& cmd{ renderer_->vulkan_util_.Begin() };
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		renderer_->vulkan_util_.Submit();

		// Clear font textures from cpu data.
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}
