#include "editor_backend.h"

#include <vector>
#include "volk.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "implot/implot.h"

#include "vulkan_renderer.h"
#include "vulkan_util.h"
#include "editor_backend.h"

namespace renderer
{
	constexpr uint32_t EDITOR_CAMERA_UBO_SET{ 0 };
	constexpr uint32_t EDITOR_RENDER_OBJECT_UBO_SET{ 1 };

	constexpr uint32_t EDITOR_OUTLINE_SET{ 0 };
	constexpr uint32_t EDITOR_MASK_TEXTURE_BINDING{ 0 };
	constexpr VkFormat MASK_COLOR_FORMAT{ VK_FORMAT_R8_UINT };

	// ImGui backend ------------------------------------------------------------------------------------------------

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
				renderer_->UpdateImages();
			}
		}
	}

	Extent ImGuiBackend::GetViewportExtent() const
	{
		return viewport_extent_;
	}

	ImageResource& ImGuiBackend::GetViewportImage()
	{
		return GetCurrentFrame().final_image;
	}

	std::array<ImageResource, FRAMES_IN_FLIGHT> ImGuiBackend::GetRasterImages()
	{
		std::array<ImageResource, FRAMES_IN_FLIGHT> raster_images{};

		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			raster_images[i] = resource.raster_image;
			++i;
		}

		return raster_images;
	}

	ImageResource& ImGuiBackend::GetViewportDepthImage()
	{
		return GetCurrentFrame().depth_image;
	}

	ImageResource& ImGuiBackend::GetRasterImage()
	{
		return GetCurrentFrame().raster_image;
	}

	std::array<ImageResource, FRAMES_IN_FLIGHT> ImGuiBackend::GetRayTraceImages()
	{
		std::array<ImageResource, FRAMES_IN_FLIGHT> rt_images{};

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
			cmd, GetCurrentFrame().raster_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		// Ray trace image is a image storage buffer which must be general layout.
		PipelineBarrier(
			cmd, GetCurrentFrame().rt_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			0, VK_ACCESS_SHADER_WRITE_BIT);

		PipelineBarrier(
			cmd, GetCurrentFrame().final_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	}

	void ImGuiBackend::TransitionColorPassesForSampling(VkCommandBuffer cmd)
	{
		// Pipeline barrier to make sure previous rendering finishes before fragment shader.
		// Also transitions image layout to be read from composite shader.
		PipelineBarrier(
			cmd, GetCurrentFrame().raster_image.image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	}

	void ImGuiBackend::TransitionFinalImageForSampling(VkCommandBuffer cmd)
	{
		PipelineBarrier(
			cmd, GetCurrentFrame().final_image.image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
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
			resource.raster_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Color attachment to render into, then ImGui samples from it.
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_FORMAT_R8G8B8A8_UNORM);
			NameObject(renderer_->context_.device, resource.raster_image.image, "ImGui_Backend_Raster_Image_" + std::to_string(i));

			resource.rt_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_FORMAT_R8G8B8A8_UNORM);
			NameObject(renderer_->context_.device, resource.rt_image.image, "ImGui_Backend_Ray_Trace_Image_" + std::to_string(i));

			resource.final_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				renderer_->swapchain_.GetImageFormat());
			NameObject(renderer_->context_.device, resource.final_image.image, "ImGui_Backend_Final_Image_" + std::to_string(i));

			resource.depth_image = renderer_->allocator_.CreateImageResource(
				extent,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				renderer_->GetDepthImageFormat(),
				VK_IMAGE_ASPECT_DEPTH_BIT);
			NameObject(renderer_->context_.device, resource.depth_image.image, "ImGui_Backend_Depth_Image_" + std::to_string(i));

			resource.render_target_descriptor = ImGui_ImplVulkan_AddTexture(
				resource.final_image.sampler,
				resource.final_image.image_view,
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
			renderer_->allocator_.DestroyImageResource(&resource.raster_image);
			renderer_->allocator_.DestroyImageResource(&resource.rt_image);
			renderer_->allocator_.DestroyImageResource(&resource.final_image);
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

	// Editor backend ------------------------------------------------------------------------------------------------

	void EditorBackend::Initialize(Context* context, VulkanRenderer* renderer)
	{
		context_ = context;
		renderer_ = renderer;
		imgui_backend_.Initialize(renderer);
		InitializeDescriptorSetLayouts();

		std::vector<DescriptorSetLayoutResource> mask_set_layouts{
			renderer_->camera_layout_resource_,
			renderer_->render_object_layout_resource_,
		};

		mask_pipeline_.Initialize(
			context,
			mask_set_layouts,
			{},
			MASK_COLOR_FORMAT,
			VK_FORMAT_UNDEFINED,
			VertexAttributes::POSITION,
			SPIRV_PREFIX / "mask.vert.spv",
			SPIRV_PREFIX / "mask.frag.spv");
		NameObject(context_->device, mask_pipeline_.pipeline, "Mask_Pipeline");
		NameObject(context_->device, mask_pipeline_.layout, "Mask_Pipeline_Layout");

		std::vector<DescriptorSetLayoutResource> outline_set_layouts{ outline_layout_resource_ };

		VkPushConstantRange color_push_constant_range{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(glm::vec4),
		};

		std::vector<VkPushConstantRange> outline_push_constant_ranges{ color_push_constant_range };

		outline_pipeline_.Initialize(
			context,
			outline_set_layouts,
			outline_push_constant_ranges,
			renderer_->swapchain_.GetImageFormat(),
			VK_FORMAT_UNDEFINED,
			VertexAttributes::NONE,
			SPIRV_PREFIX / "fullscreen_triangle.vert.spv",
			SPIRV_PREFIX / "outline.frag.spv");
		NameObject(context_->device, outline_pipeline_.pipeline, "Outline_Pipeline");
		NameObject(context_->device, outline_pipeline_.layout, "Outline_Pipeline_Layout");

		InitializeFrameResources();
	}

	void EditorBackend::CleanUp()
	{
		imgui_backend_.CleanUp();

		mask_pipeline_.CleanUp();
		outline_pipeline_.CleanUp();

		for (FrameResources& resource : frame_resources_) {
			renderer_->allocator_.DestroyImageResource(&resource.mask_image);
		}

		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&outline_layout_resource_);
	}

	void EditorBackend::InitializeDescriptorSetLayouts()
	{
		VkDescriptorSetLayoutBinding mask_binding{
			.binding = EDITOR_MASK_TEXTURE_BINDING ,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> outline_bindings{
			mask_binding,
		};

		outline_layout_resource_ = renderer_->descriptor_allocator_.CreateDescriptorSetLayoutResource(outline_bindings);
		NameObject(context_->device, outline_layout_resource_.layout, "Outline_Layout_Resource");
	}

	ImGuiBackend& EditorBackend::GetImGuiBackend()
	{
		return imgui_backend_;
	}

	const ImGuiBackend& EditorBackend::GetImGuiBackend() const
	{
		return imgui_backend_;
	}

	void EditorBackend::SetViewportSize(const Extent& extent)
	{
		imgui_backend_.SetViewportSize(extent);

		DestroyFrameImages();
		CreateFrameImages();
	}

	void EditorBackend::EditorRenderPasses(VkCommandBuffer cmd)
	{
		for (const OutlineObjects& outline_set : outline_objects_)
		{
			// Transition mask image to render to as color attachment.
			PipelineBarrier(cmd, GetCurrentFrame().mask_image.image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);

			MaskRenderPass(cmd, outline_set);

			// No transitions needed here, just barrier for writing to final image.
			PipelineBarrier(cmd, imgui_backend_.GetViewportImage().image,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);

			// Transition mask image to be sampled as texture.
			PipelineBarrier(cmd, GetCurrentFrame().mask_image.image,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);

			OutlineRenderPass(cmd, outline_set);
		}
	}

	void EditorBackend::AddOutlineSet(std::vector<uint32_t>&& selection_set, const glm::vec4& color)
	{
		OutlineObjects& outline_set{ outline_objects_.emplace_back() };
		outline_set.render_object_indices = std::move(selection_set);
		outline_set.color = color;
	}

	void EditorBackend::ClearOutlineSets()
	{
		outline_objects_.clear();
	}

	EditorBackend::FrameResources& EditorBackend::GetCurrentFrame()
	{
		return frame_resources_[renderer_->current_frame_];
	}

	void EditorBackend::InitializeFrameResources()
	{
		// We don't create images yet since the viewport size isn't set until the imgui viewport is drawn.
		for (FrameResources& resource : frame_resources_)
		{
			resource.outline_set_resource_ = renderer_->descriptor_allocator_.CreateDescriptorSetResource(outline_layout_resource_);
			NameObject(context_->device, resource.outline_set_resource_.descriptor_set, "Outline_Descriptor_Set");
		}
	}

	void EditorBackend::CreateFrameImages()
	{
		for (FrameResources& resource : frame_resources_)
		{
			resource.mask_image = renderer_->allocator_.CreateImageResource(
				renderer_->GetViewportExtent(),
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				MASK_COLOR_FORMAT);
			NameObject(context_->device, resource.mask_image.image, "Outline_Mask_Image");
			NameObject(context_->device, resource.mask_image.image_view, "Outline_Mask_Image_View");

			resource.outline_set_resource_.LinkImageToBinding(EDITOR_MASK_TEXTURE_BINDING, resource.mask_image, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	void EditorBackend::DestroyFrameImages()
	{
		for (FrameResources& resource : frame_resources_) {
			renderer_->allocator_.DestroyImageResource(&resource.mask_image);
		}
	}

	void EditorBackend::MaskRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set)
	{
		Extent viewport_extents{ renderer_->GetViewportExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.0f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = GetCurrentFrame().mask_image.image_view,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_color,
		};

		VkRenderingInfo rendering_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = 0,
			.renderArea = {
				.offset = { 0, 0 },
				.extent = { viewport_extents.width, viewport_extents.height },
			},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &rendering_info);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			mask_pipeline_.layout,
			EDITOR_CAMERA_UBO_SET,
			1,
			&renderer_->GetCurrentFrame().camera_descriptor_set_resource.descriptor_set,
			0,
			nullptr);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mask_pipeline_.pipeline);

		VkDeviceSize zero_offset{ 0 };

		for (uint32_t render_object_index : outline_set.render_object_indices)
		{
			RenderObject* render_object{ renderer_->GetCurrentFrame().render_objects[render_object_index] };

			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				mask_pipeline_.layout,
				EDITOR_RENDER_OBJECT_UBO_SET,
				1,
				&render_object->ubo_descriptor_set_resource.descriptor_set,
				0,
				nullptr);

			for (auto& geometry : renderer_->meshes_[render_object->mesh_idx]->geometries)
			{
				VkIndexType index_type{ std::is_same<uint32_t, decltype(Geometry::indices)::value_type>::value ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &geometry.vertices_resource.buffer, &zero_offset);
				vkCmdBindIndexBuffer(cmd, geometry.indices_resource.buffer, 0, index_type);
				vkCmdDrawIndexed(cmd, (uint32_t)geometry.indices.size(), 1, 0, 0, 0);
			}
		}
		vkCmdEndRendering(cmd);
	}

	void EditorBackend::OutlineRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set)
	{
		Extent viewport_extents{ renderer_->GetViewportExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.0f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = imgui_backend_.GetViewportImage().image_view,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_color,
		};

		VkRenderingInfo rendering_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = 0,
			.renderArea = {
				.offset = { 0, 0 },
				.extent = { viewport_extents.width, viewport_extents.height },
			},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &rendering_info);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline_pipeline_.layout, EDITOR_OUTLINE_SET, 1, &GetCurrentFrame().outline_set_resource_.descriptor_set, 0, nullptr);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline_pipeline_.pipeline);
		vkCmdPushConstants(cmd, outline_pipeline_.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &outline_set.color);

		// Fullscreen triangle.
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRendering(cmd);
	}
}
