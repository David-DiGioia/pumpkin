#include "imgui_backend.h"

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
	void ImGuiBackend::Initialize(VulkanRenderer* renderer)
	{
		renderer_ = renderer;

		CreateDescriptorPool();
		InitializeImGui();

		// Call editor's custom initialization function.
		callbacks_.initialization_callback(callbacks_.user_data);
	}

	void ImGuiBackend::CleanUp()
	{
		DestroyFrameResources();
		vkDestroyDescriptorPool(renderer_->context_.device, descriptor_pool_, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void ImGuiBackend::DrawGui()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Call editor's custom gui callback.
		// The callback will update the viewport extent here if it needs to.
		callbacks_.gui_callback((ImTextureID*)&GetCurrentFrame().render_target_descriptor, callbacks_.user_data);

		ImGui::Render();
	}

	void ImGuiBackend::RecordCommandBuffer(VkCommandBuffer cmd)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	void ImGuiBackend::SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks)
	{
		callbacks_ = imgui_callbacks;
	}

	void ImGuiBackend::SetViewportSize(const Extent& extent)
	{
		if (extent != viewport_extent_)
		{
			viewport_extent_ = extent;
			viewport_visible_ = (extent.width != 0) && (extent.height != 0);

			if (viewport_visible_)
			{
				DestroyFrameResources();
				CreateFrameResources(extent);
				renderer_->SetRayTraceImages(GetRayTraceImages());
			}
		}
	}

	Extent ImGuiBackend::GetViewportExtent() const
	{
		return viewport_extent_;
	}

	ImageResource& ImGuiBackend::GetViewportImage()
	{
		return GetCurrentFrame().render_image;
	}

	ImageResource& ImGuiBackend::GetViewportDepthImage()
	{
		return GetCurrentFrame().depth_image;
	}

	std::array<ImageResource, FRAMES_IN_FLIGHT> ImGuiBackend::GetRayTraceImages()
	{
		std::array<ImageResource, FRAMES_IN_FLIGHT> rt_images;

		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			rt_images[i] = resource.rt_image;
			++i;
		}

		return rt_images;
	}

	void ImGuiBackend::TransitionImagesForRender(VkCommandBuffer cmd)
	{
		// If we're using the editor's image we need to transition it to be a color attachment.
		PipelineBarrier(
			cmd, GetCurrentFrame().render_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		// Ray trace image is a image storage buffer which must be general layout.
		PipelineBarrier(
			cmd, GetCurrentFrame().rt_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			0, VK_ACCESS_SHADER_WRITE_BIT);
	}

	void ImGuiBackend::TransitionImagesForSampling(VkCommandBuffer cmd)
	{
		// Pipeline barrier to make sure previous rendering finishes before fragment shader.
		// Also transitions image layout to be read from shader.
		PipelineBarrier(
			cmd, GetCurrentFrame().render_image.image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		PipelineBarrier(
			cmd, GetCurrentFrame().rt_image.image,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	}

	bool ImGuiBackend::GetViewportVisible() const
	{
		return viewport_visible_;
	}

	ImGuiBackend::FrameResources& ImGuiBackend::GetCurrentFrame()
	{
		return frame_resources_[renderer_->current_frame_];
	}

	void ImGuiBackend::CreateFrameResources(Extent extent)
	{
		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			resource.render_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Color attachment to render into, then ImGui samples from it.
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				renderer_->swapchain_.GetImageFormat());
			NameObject(renderer_->context_.device, resource.render_image.image, "ImGui_Backend_Render_Image_" + std::to_string(i));

			resource.rt_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_FORMAT_B8G8R8A8_UNORM);
			NameObject(renderer_->context_.device, resource.rt_image.image, "ImGui_Backend_Ray_Trace_Image_" + std::to_string(i));

			resource.depth_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				renderer_->GetDepthImageFormat(),
				VK_IMAGE_ASPECT_DEPTH_BIT);
			NameObject(renderer_->context_.device, resource.depth_image.image, "ImGui_Backend_Depth_Image_" + std::to_string(i));

			resource.render_target_descriptor = ImGui_ImplVulkan_AddTexture(
				resource.render_image.sampler,
				resource.render_image.image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			NameObject(renderer_->context_.device, resource.render_target_descriptor, "ImGui_Backend_Render_Target_Descriptor_" + std::to_string(i));

			// Transition depth image.
			renderer_->vulkan_util_.Begin();
			renderer_->vulkan_util_.PipelineBarrier(
				resource.depth_image.image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0, 0,
				VK_IMAGE_ASPECT_DEPTH_BIT);
			renderer_->vulkan_util_.Submit();

			++i;
		}
	}

	void ImGuiBackend::DestroyFrameResources()
	{
		VkResult result{ vkDeviceWaitIdle(renderer_->context_.device) };
		CheckResult(result, "Error waiting for device to idle.");

		for (FrameResources& resource : frame_resources_)
		{
			renderer_->allocator_.DestroyImageResource(&resource.render_image);
			renderer_->allocator_.DestroyImageResource(&resource.rt_image);
			renderer_->allocator_.DestroyImageResource(&resource.depth_image);

			result = vkFreeDescriptorSets(renderer_->context_.device, descriptor_pool_, 1, &resource.render_target_descriptor);
			CheckResult(result, "Failed to free descriptor set.");
			resource.render_target_descriptor = VK_NULL_HANDLE;
		}
	}

	void ImGuiBackend::CreateDescriptorPool()
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
		NameObject(renderer_->context_.device, descriptor_pool_, "Imgui_Descriptor_Pool");
	}

	void ImGuiBackend::InitializeImGui()
	{
		// This initializes the core structures of imgui
		ImGui::CreateContext();
		ImPlot::CreateContext();

		// TODO: Once ImGui has pr merged, enable this.
		//ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

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

		// Execute a GPU command to upload imgui font textures.
		auto& cmd{ renderer_->vulkan_util_.Begin() };
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		renderer_->vulkan_util_.Submit();

		// Clear font textures from cpu data.
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}
