#include "vulkan_renderer.h"

#include <fstream>

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "imgui.h"
#include "tracy/Tracy.hpp"

#include "vulkan_util.h"
#include "logger.h"
#include "context.h"
#include "mesh.h"
#include "descriptor_set.h"

namespace jsonkey {
	const std::string RENDER_OBJECTS{ "render_objects" };
	// Render object members.
	const std::string MESH_INDEX{ "mesh_index" };
	const std::string MATERIAL_INDICES{ "material_indices" };
	// End render object members.

	const std::string MESHES{ "meshes" };
	// Mesh members.
	const std::string GEOMETRIES{ "geometries" };
	// Geometry members.
	const std::string VERTEX_BYTE_OFFSET{ "vertex_byte_offset" };
	const std::string VERTEX_BYTE_SIZE{ "vertex_byte_size" };
	const std::string INDEX_BYTE_OFFSET{ "index_byte_offset" };
	const std::string INDEX_BYTE_SIZE{ "index_byte_size" };
	// End geometry members.
	// End mesh members.

	const std::string MATERIALS{ "materials" };
	// Material members.
	const std::string COLOR{ "color" };
	const std::string METALLIC{ "metallic" };
	const std::string ROUGHNESS{ "roughness" };
	const std::string EMISSION{ "emission" };
	const std::string IOR{ "ior" };
	const std::string COLOR_INDEX{ "color_index" };
	const std::string METALLIC_INDEX{ "metallic_index" };
	const std::string ROUGHNESS_INDEX{ "roughness_index" };
	const std::string EMISSION_INDEX{ "emission_index" };
	const std::string NORMAL_INDEX{ "normal_index" };
	// End material members.

	const std::string TEXTURES{ "textures" };
	// Texture members.
	const std::string WIDTH{ "width" };
	const std::string HEIGHT{ "height" };
	const std::string CHANNELS{ "channels" };
	const std::string COLOR_DATA{ "non_color" };
	const std::string TEXTURE_INDEX{ "texture_index" };
	const std::string TEXTURE_BYTE_OFFSET{ "texture_byte_offset" };
	// End texture members.

	const std::string MESH_HASH_MAP{ "mesh_hash_map" };
	// Mesh hash map members.
	const std::string VERTEX_HASH{ "vertex_hash" };
	const std::string INDEX_HASH{ "index_hash" };
	const std::string HASH_MAP_MESH_INDEX{ "hash_map_mesh_index" };
	// End mesh hash map members.
}

// Make it so glm::vec4 can be serialized with json library.
namespace glm
{
	void to_json(nlohmann::json& j, const glm::vec4& v)
	{
		j = { { "x", v.x }, { "y", v.y }, { "z", v.z }, { "w", v.w } };
	};

	void from_json(const nlohmann::json& j, glm::vec4& v)
	{
		v.x = j.at("x").get<float>();
		v.y = j.at("y").get<float>();
		v.z = j.at("z").get<float>();
		v.w = j.at("w").get<float>();
	}
}

namespace renderer
{
	// Camera descriptor set.
	constexpr uint32_t CAMERA_UBO_SET{ 0 };
	constexpr uint32_t CAMERA_UBO_BINDING{ 0 };
	// Render object descriptor set.
	constexpr uint32_t RENDER_OBJECT_UBO_SET{ 1 };
	constexpr uint32_t RENDER_OBJECT_UBO_BINDING{ 0 };
	// Composite pass descriptor set.
	constexpr uint32_t COMPOSITE_DESCRIPTOR_SET{ 0 };
	constexpr uint32_t COMPOSITE_RASTER_BINDING{ 0 };
	constexpr uint32_t COMPOSITE_RT_IMAGE_BINDING{ 1 };

	constexpr VkFormat COLOR_TEXTURE_FORMAT{ VK_FORMAT_R8G8B8A8_SRGB };
	constexpr VkFormat NON_COLOR_TEXTURE_FORMAT{ VK_FORMAT_R8G8B8A8_UNORM };

	// Choose default values for material. Later maybe should take more values from gltf material.
	const Material default_material{
		.color = glm::vec4{ 0.4f, 0.4f, 0.4f, 1.0f },
		.metallic = 0.0f,
		.roughness = 0.8f,
		.emission = 0.0f,
		.ior = 1.53f,
		.color_index = NULL_INDEX,
		.metallic_index = NULL_INDEX,
		.roughness_index = NULL_INDEX,
		.emission_index = NULL_INDEX,
		.normal_index = NULL_INDEX,
	};

	void WindowResizedCallback(GLFWwindow* window, int width, int height)
	{
		auto renderer{ reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window)) };
		renderer->WindowResized();
	}

	void VulkanRenderer::Initialize(GLFWwindow* window)
	{
		VkResult result{ volkInitialize() };
		CheckResult(result, "Failed to initialize volk.");

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, WindowResizedCallback);

		context_.Initialize(window);
		swapchain_.Initialize(&context_);
		descriptor_allocator_.Initialize(&context_);
		InitializeDescriptorSetLayouts();
		InitializePipelines();
		allocator_.Initialize(&context_);
		vulkan_util_.Initialize(&context_, &allocator_);
		InitializeFrameResources();
		particle_context_.Initialize(&context_, this);
		InitializeRayTracing();

		std::array<std::vector<RenderObject*>*, FRAMES_IN_FLIGHT> render_object_resources{};
		for (uint32_t i{ 0 }; i < FRAMES_IN_FLIGHT; ++i) {
			render_object_resources[i] = &frame_resources_[i].render_objects;
		}
		std::function<void(RenderObject*, bool)> render_object_destroyer{ [&](RenderObject* ro_ptr, bool last_resource) {
			DestroyRenderObject(ro_ptr, last_resource);
		} };
		render_object_destroyer_.Initialize(std::move(render_object_resources), render_object_destroyer, current_frame_);

#ifdef EDITOR_ENABLED
		editor_backend_.Initialize(
			particle_context_.GetParticleVertices(),
			particle_context_.GetParticleIndices(),
			&context_, this);
#endif
	}

#ifdef EDITOR_ENABLED
	void VulkanRenderer::SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks)
	{
		editor_backend_.GetImGuiBackend().SetImGuiCallbacks(imgui_callbacks);
	}

	void VulkanRenderer::SetImGuiViewportSize(const Extent& extent)
	{
		editor_backend_.SetViewportSize(extent);
	}

	void VulkanRenderer::AddOutlineSet(const std::vector<RenderObjectHandle>& selection_set, const glm::vec4& color)
	{
		std::vector<uint32_t> transformed_selection_set(selection_set.size());
		std::transform(selection_set.begin(), selection_set.end(), transformed_selection_set.begin(), [&](renderer::RenderObjectHandle handle) {
			return (uint32_t)handle;
			});
		editor_backend_.AddOutlineSet(std::move(transformed_selection_set), color);
	}

	void VulkanRenderer::SetParticleOverlayEnabled(bool render_grid, bool render_nodes, bool rasterize_particles, bool use_particle_depth)
	{
		particle_context_.SetMPMDebugParticleGenEnabled(rasterize_particles);
		particle_context_.SetMPMDebugNodeGenEnabled(render_nodes);
		editor_backend_.SetRasterParticlesEnabled(rasterize_particles);
		editor_backend_.SetNodesEnabled(render_nodes);
		editor_backend_.SetGridEnabled(render_grid);
		editor_backend_.SetParticleDepthEnabled(use_particle_depth);
	}

	void VulkanRenderer::SetParticleOverlay(RenderObjectHandle render_object)
	{
		editor_backend_.SetRenderObjectInfo(CHUNK_WIDTH, (uint32_t)render_object);
	}

	void VulkanRenderer::ClearOutlineSets()
	{
		editor_backend_.ClearOutlineSets();
	}

	void VulkanRenderer::SetParticleColorMode(uint32_t color_mode)
	{
		editor_backend_.SetParticleColorMode(color_mode);
	}

	void VulkanRenderer::SetParticleColorModeMaxValue(float max_value)
	{
		editor_backend_.SetParticleColorModeMaxValue(max_value);
	}

	void VulkanRenderer::SetNodeColorMode(uint32_t color_mode)
	{
		editor_backend_.SetNodeColorMode(color_mode);
	}

	void VulkanRenderer::SetNodeColorModeMaxValue(float max_value)
	{
		editor_backend_.SetNodeColorModeMaxValue(max_value);
	}

	void VulkanRenderer::SetRenderCubeNodesEnabled(bool enabled)
	{
		editor_backend_.SetRenderCubeNodesEnabled(enabled);
	}
#endif

	void VulkanRenderer::CleanUp()
	{
		VkResult result{ vkDeviceWaitIdle(context_.device) };
		CheckResult(result, "Error waiting for device to idle.");

#ifdef EDITOR_ENABLED
		editor_backend_.CleanUp();
#endif

		rt_context_.CleanUp();

		for (FrameResources& frame : frame_resources_)
		{
			vkDestroyFence(context_.device, frame.render_done_fence, nullptr);
			vkDestroySemaphore(context_.device, frame.image_acquired_semaphore, nullptr);
			vkDestroySemaphore(context_.device, frame.render_done_semaphore, nullptr);

			for (RenderObject* render_object : frame.render_objects)
			{
				if (render_object)
				{
					allocator_.DestroyBufferResource(&render_object->ubo_buffer_resource);
					delete render_object;
				}
			}
			allocator_.DestroyBufferResource(&frame.camera_ubo_buffer);

			if (frame.tlas)
			{
				vkDestroyAccelerationStructureKHR(context_.device, frame.tlas->acceleration_structure, nullptr);
				allocator_.DestroyBufferResource(&frame.tlas->buffer_resource);
			}
		}

		for (ImageResource* texture : textures_) {
			allocator_.DestroyImageResource(texture);
		}

		descriptor_allocator_.DestroyDescriptorSetLayoutResource(&render_object_layout_resource_);
		descriptor_allocator_.DestroyDescriptorSetLayoutResource(&camera_layout_resource_);
		descriptor_allocator_.DestroyDescriptorSetLayoutResource(&composite_layout_resource_);

		for (Material* material : materials_) {
			delete material;
		}
		materials_.clear();

		for (uint32_t mesh_idx{ 0 }; mesh_idx < (uint32_t)meshes_.size(); ++mesh_idx) {
			DestroyMesh(mesh_idx);
		}
		meshes_.clear();

		// Destroying command pool frees all command buffers allocated from it.
		vkDestroyCommandPool(context_.device, command_pool_, nullptr);

		particle_context_.CleanUp();
		vulkan_util_.CleanUp();
		allocator_.CleanUp();
		raster_pipeline_.CleanUp();
		composite_pipeline_.CleanUp();

		for (ComputePipeline* compute_shader : user_compute_shaders_)
		{
			compute_shader->CleanUp();
			delete compute_shader;
		}
		user_compute_shaders_.clear();

		descriptor_allocator_.CleanUp();
		swapchain_.CleanUp();
		context_.CleanUp();
	}

	void VulkanRenderer::WindowResized()
	{
		Extent extents{ context_.GetWindowExtent() };

		// If window is minimized, just wait here until it is unminimized.
		while (extents.width == 0 || extents.height == 0)
		{
			extents = context_.GetWindowExtent();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(context_.device);
		swapchain_.CleanUp();
		swapchain_.Initialize(&context_);
		//InitializeRayTraceImages();
	}

	void VulkanRenderer::InitializePipelines()
	{
		std::vector<DescriptorSetLayoutResource> raster_layouts{
			camera_layout_resource_,
			render_object_layout_resource_,
		};

		raster_pipeline_.Initialize(
			&context_,
			raster_layouts,
			{},
			VK_FORMAT_R8G8B8A8_UNORM,
			GetDepthImageFormat(),
			VertexAttributes::POSITION_NORMAL,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			SPIRV_PREFIX / "default.vert.spv",
			SPIRV_PREFIX / "default.frag.spv");
		NameObject(context_.device, raster_pipeline_.pipeline, "Raster_Pipeline");
		NameObject(context_.device, raster_pipeline_.layout, "Raster_Pipeline_Layout");

		std::vector<DescriptorSetLayoutResource> composite_layouts{
			composite_layout_resource_,
		};

		composite_pipeline_.Initialize(
			&context_,
			composite_layouts,
			{},
			GetViewportImageFormat(),
			VK_FORMAT_UNDEFINED,
			VertexAttributes::NONE,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			SPIRV_PREFIX / "fullscreen_triangle.vert.spv",
			SPIRV_PREFIX / "composite.frag.spv");
		NameObject(context_.device, composite_pipeline_.pipeline, "Composite_Pipeline");
		NameObject(context_.device, composite_pipeline_.layout, "Composite_Pipeline_Layout");
	}

	void VulkanRenderer::InitializeRayTracing()
	{
		rt_context_.Initialize(&context_, this, &allocator_, &descriptor_allocator_, &vulkan_util_);
	}

	VkFormat VulkanRenderer::GetDepthImageFormat() const
	{
		return VK_FORMAT_D32_SFLOAT;
	}

	void VulkanRenderer::WaitForLastFrame()
	{
		// Wait until the GPU has finished rendering the last frame to use these resources. Timeout of 1 second.
		VkResult result{ vkWaitForFences(context_.device, 1, &GetCurrentFrame().render_done_fence, VK_TRUE, 1'000'000'000) };
		CheckResult(result, "Error waiting for render_fence.");
		result = vkResetFences(context_.device, 1, &GetCurrentFrame().render_done_fence);
		CheckResult(result, "Error resetting render_fence.");
	}

	void VulkanRenderer::Render()
	{
		ZoneScoped;
		// Request image from the swapchain, one second timeout.
		// This is also where vsync happens according to vkguide, but for me it happens at present.
		uint32_t image_index{ swapchain_.AcquireNextImage(GetCurrentFrame().image_acquired_semaphore) };

		// Now that we are sure that the commands finished executing, we can safely
		// reset the command buffer to begin recording again.
		VkResult result{ vkResetCommandBuffer(GetCurrentFrame().command_buffer, 0) };
		CheckResult(result, "Error resetting command buffer.");

#ifdef EDITOR_ENABLED
		editor_backend_.GetImGuiBackend().DrawGui();
#endif

		// Drawing commands happen here.
		RecordCommandBuffer(GetCurrentFrame().command_buffer, image_index);

		VkPipelineStageFlags wait_stage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		// We can't start rendering until image_acquired_semaphore is signaled,
		// meaning the image is ready to be used.
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &GetCurrentFrame().image_acquired_semaphore,
			.pWaitDstStageMask = &wait_stage,
			.commandBufferCount = 1,
			.pCommandBuffers = &GetCurrentFrame().command_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &GetCurrentFrame().render_done_semaphore,
		};

		// Submit command buffer to the queue and execute it.
		// render_done_fence will now block until the graphic commands finish execution.
		result = vkQueueSubmit(context_.graphics_queue, 1, &submit_info, GetCurrentFrame().render_done_fence);
		CheckResult(result, "Error submitting queue.");

#ifdef EDITOR_ENABLED
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
#endif

		// This will put the image we just rendered into the visible window.
		// We want to wait on render_done_semaphore for that, as it's necessary that the
		// drawing commands have finished before the image is displayed to the user.
		VkPresentInfoKHR present_info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &GetCurrentFrame().render_done_semaphore,
			.swapchainCount = 1,
			.pSwapchains = &swapchain_.GetSwapchain(),
			.pImageIndices = &image_index,
			.pResults = nullptr,
		};

		// This is what blocks for vsync on my gtx 1080.
		result = vkQueuePresentKHR(context_.graphics_queue, &present_info);
		CheckResult(result, "Error presenting image.");

		FrameMark;
		NextFrame();
	}

	void VulkanRenderer::Draw(VkCommandBuffer cmd, uint32_t image_index)
	{
		if (!GetViewportMinimized())
		{
			// Ray tracing render pass.
			if (GetCurrentFrame().tlas) {
				rt_context_.Render(cmd);
			}

			// First raster render pass. Render 3D Viewport.
			RasterRenderPass(cmd);

			// Transition rt image and raster image to be sampled during composite pass.
			editor_backend_.GetImGuiBackend().TransitionColorPassesForSampling(cmd);

			// Composite all render passes except editor render passes, which will write directly to the final image.
			CompositeRenderPass(cmd, image_index);
		}

#ifdef EDITOR_ENABLED
		if (!GetViewportMinimized())
		{
			// Multiple editor render passes. Render editor-specific graphics in the viewport like outline of selected.
			editor_backend_.EditorRenderPasses(cmd);

			// Transition composited image to be sampled from ImGui renderpass.
			editor_backend_.GetImGuiBackend().TransitionFinalImageForSampling(cmd);
		}

		// Last render pass. Render editor GUI.
		EditorGuiRenderPass(cmd, image_index);
#endif
	}

	void VulkanRenderer::RasterRenderPass(VkCommandBuffer cmd)
	{
		Extent viewport_extents{ GetViewportExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.2f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = editor_backend_.GetImGuiBackend().GetRasterImage().image_view,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_color,
		};

		VkRenderingAttachmentInfo depth_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = GetViewportDepthImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_NONE,
			.clearValue = VkClearColorValue{1.0f, 1.0f, 1.0f, 1.0f},
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
			.pDepthAttachment = &depth_attachment_info,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &rendering_info);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, raster_pipeline_.layout, CAMERA_UBO_SET, 1, &GetCurrentFrame().camera_descriptor_set_resource.descriptor_set, 0, nullptr);

		VkDeviceSize zero_offset{ 0 };

		for (RenderObject* render_obj : GetCurrentFrame().render_objects)
		{
			if (!render_obj) {
				continue;
			}

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, raster_pipeline_.pipeline);
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				raster_pipeline_.layout,
				RENDER_OBJECT_UBO_SET,
				1,
				&render_obj->ubo_descriptor_set_resource.descriptor_set,
				0,
				nullptr);

			for (auto& geometry : meshes_[render_obj->mesh_idx]->geometries)
			{
				VkIndexType index_type{ std::is_same<uint32_t, decltype(Geometry::indices)::value_type>::value ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &geometry.vertices_resource.buffer, &zero_offset);
				vkCmdBindIndexBuffer(cmd, geometry.indices_resource.buffer, 0, index_type);
				vkCmdDrawIndexed(cmd, (uint32_t)geometry.indices.size(), 1, 0, 0, 0);
			}
		}

		vkCmdEndRendering(cmd);
	}

	void VulkanRenderer::EditorGuiRenderPass(VkCommandBuffer cmd, uint32_t image_index)
	{
		Extent window_extent{ context_.GetWindowExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.2f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = swapchain_.GetImageView(image_index),
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
				.extent = { window_extent.width, window_extent.height },
			},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &rendering_info);
		editor_backend_.GetImGuiBackend().RecordCommandBuffer(cmd);
		vkCmdEndRendering(cmd);
	}

	void VulkanRenderer::CompositeRenderPass(VkCommandBuffer cmd, uint32_t image_index)
	{
		Extent viewport_extents{ GetViewportExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.0f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = GetViewportImageView(image_index),
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

		vkCmdBindDescriptorSets(
			cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
			composite_pipeline_.layout, COMPOSITE_DESCRIPTOR_SET,
			1, &GetCurrentFrame().composite_descriptor_set_resource.descriptor_set,
			0, nullptr);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, composite_pipeline_.pipeline);

		// Fullscreen triangle.
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRendering(cmd);
	}

	void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index)
	{
		// Begin the command buffer recording. We will use this command buffer exactly once,
		// so we want to let Vulkan know that.
		VkCommandBufferBeginInfo command_buffer_begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		Extent viewport_extent{ GetViewportExtent() };

		VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)viewport_extent.width,
			.height = (float)viewport_extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		VkRect2D scissor{
			.offset = {0, 0},
			.extent = {viewport_extent.width, viewport_extent.height},
		};

		VkResult result{ vkBeginCommandBuffer(cmd, &command_buffer_begin_info) };
		CheckResult(result, "Failed to begin command buffer.");

		// If the editor viewport is minimized we don't set viewport/scissors.
#ifdef EDITOR_ENABLED
		bool minimized{ !editor_backend_.GetImGuiBackend().GetViewportVisible() };
#else
		constexpr bool minimized{ false };
#endif

		if (!minimized)
		{
			// We only need to set these for the 3D viewport render because the ImGui
			// implementation sets them again for the GUI render pass.
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);
		}

		TransitionImagesForRender(cmd, image_index);
		Draw(cmd, image_index);
		TransitionSwapImageForPresent(cmd, image_index);

		result = vkEndCommandBuffer(GetCurrentFrame().command_buffer);
		CheckResult(result, "Failed to end command buffer.");
	}

	void VulkanRenderer::TransitionImagesForRender(VkCommandBuffer cmd, uint32_t image_index)
	{
#ifdef EDITOR_ENABLED
		if (!GetViewportMinimized()) {
			editor_backend_.GetImGuiBackend().TransitionImagesForRender(cmd);
		}
#endif
		PipelineBarrier(
			cmd, swapchain_.GetImage(image_index),
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,              // Image layout.
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Pipeline stages.
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,                                          // Access types.
			VK_IMAGE_ASPECT_COLOR_BIT);                                                       // Aspect mask.
	}

	void VulkanRenderer::TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index)
	{
		PipelineBarrier(
			cmd, swapchain_.GetImage(image_index),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,           // Image layout.
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Pipeline stages.
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,                                             // Access types.
			VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void VulkanRenderer::NextFrame()
	{
		current_frame_ = (current_frame_ + 1) % FRAMES_IN_FLIGHT;
	}

	VulkanRenderer::FrameResources& VulkanRenderer::GetCurrentFrame()
	{
		return frame_resources_[current_frame_];
	}

	const VulkanRenderer::FrameResources& VulkanRenderer::GetCurrentFrame() const
	{
		return frame_resources_[current_frame_];
	}

	Extent VulkanRenderer::GetViewportExtent()
	{
#ifdef EDITOR_ENABLED
		return editor_backend_.GetImGuiBackend().GetViewportExtent();
#else
		return context_.GetWindowExtent();
#endif
	}

	void VulkanRenderer::DumpRenderData(
		nlohmann::json& j,
		const std::filesystem::path& vertex_path,
		const std::filesystem::path& index_path,
		const std::filesystem::path& texture_path)
	{
		// Save mesh hash map.
		for (const auto& pair : mesh_hash_map_)
		{
			j[jsonkey::MESH_HASH_MAP] += {
				{ jsonkey::VERTEX_HASH, pair.first },
				{ jsonkey::INDEX_HASH, pair.second.first },
				{ jsonkey::HASH_MAP_MESH_INDEX, pair.second.second },
			};
		}

		// Save render objects.
		for (RenderObject* ro : GetCurrentFrame().render_objects)
		{
			j[jsonkey::RENDER_OBJECTS] += {
				{ jsonkey::MESH_INDEX, ro->mesh_idx },
				{ jsonkey::MATERIAL_INDICES, ro->material_indices },
			};
		}

		uint32_t vertex_byte_offest{ 0 };
		uint32_t index_byte_offset{ 0 };
		uint32_t texture_byte_offset{ 0 };

		std::ofstream vertex_file{ vertex_path, std::ios::out | std::ios::binary };
		std::ofstream index_file{ index_path, std::ios::out | std::ios::binary };
		std::ofstream texture_file{ texture_path, std::ios::out | std::ios::binary };

		// Save meshes.
		for (const Mesh* mesh : meshes_)
		{
			nlohmann::json json_mesh{};

			for (const Geometry& geometry : mesh->geometries)
			{
				json_mesh[jsonkey::GEOMETRIES] += {
					{ jsonkey::VERTEX_BYTE_OFFSET, vertex_byte_offest },
					{ jsonkey::VERTEX_BYTE_SIZE, geometry.vertices.size() * sizeof(Vertex) },
					{ jsonkey::INDEX_BYTE_OFFSET, index_byte_offset },
					{ jsonkey::INDEX_BYTE_SIZE, geometry.indices.size() * sizeof(decltype(Geometry::indices)::value_type) },
				};

				size_t vertex_byte_size{ geometry.vertices.size() * sizeof(Vertex) };
				size_t index_byte_size{ geometry.indices.size() * sizeof(decltype(Geometry::indices)::value_type) };

				vertex_file.write(reinterpret_cast<const char*>(geometry.vertices.data()), vertex_byte_size);
				index_file.write(reinterpret_cast<const char*>(geometry.indices.data()), index_byte_size);

				vertex_byte_offest += (uint32_t)(vertex_byte_size);
				index_byte_offset += (uint32_t)(index_byte_size);
			}

			j[jsonkey::MESHES] += json_mesh;
		}

		// Save textures.
		uint32_t texture_idx{ 0 };
		for (const ImageResource* texture : textures_)
		{
			constexpr uint32_t channels{ 4 };

			j[jsonkey::TEXTURES] += {
				{ jsonkey::WIDTH, texture->extent.width },
				{ jsonkey::HEIGHT, texture->extent.height },
				{ jsonkey::CHANNELS, channels },
				{ jsonkey::COLOR_DATA, texture->format == COLOR_TEXTURE_FORMAT },
				{ jsonkey::TEXTURE_INDEX, texture_idx++ },
				{ jsonkey::TEXTURE_BYTE_OFFSET, texture_byte_offset },
			};

			VkDeviceSize byte_size{ (VkDeviceSize)texture->extent.width * texture->extent.height * channels };
			BufferResource host_buffer{ allocator_.CreateBufferResource(byte_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };
			NameObject(context_.device, host_buffer.buffer, "Texture_Temporary_Host_Buffer");

			vulkan_util_.Begin();
			// Transition layout for transfer.
			vulkan_util_.PipelineBarrier(
				texture->image,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
			vulkan_util_.TransferImageToBuffer(host_buffer, byte_size, *texture, texture->extent.width, texture->extent.height);
			vulkan_util_.Submit();

			void* data{};
			vkMapMemory(context_.device, *host_buffer.memory, host_buffer.offset, host_buffer.size, 0, &data);

			texture_file.write(reinterpret_cast<const char*>(data), byte_size);
			texture_byte_offset += byte_size;

			vkUnmapMemory(context_.device, *host_buffer.memory);
			allocator_.DestroyBufferResource(&host_buffer);
		}

		// Save materials.
		for (const Material* material : materials_)
		{
			j[jsonkey::MATERIALS] += {
				{ jsonkey::COLOR, material->color },
				{ jsonkey::METALLIC, material->metallic },
				{ jsonkey::ROUGHNESS, material->roughness },
				{ jsonkey::EMISSION, material->emission },
				{ jsonkey::IOR, material->ior },
				{ jsonkey::COLOR_INDEX, material->color_index },
				{ jsonkey::METALLIC_INDEX, material->metallic_index },
				{ jsonkey::ROUGHNESS_INDEX, material->roughness_index },
				{ jsonkey::EMISSION_INDEX, material->emission_index },
				{ jsonkey::NORMAL_INDEX, material->normal_index },
			};
		}

		vertex_file.close();
		index_file.close();
		texture_file.close();
	}

	void VulkanRenderer::LoadRenderData(
		nlohmann::json& j,
		const std::filesystem::path& vertex_path,
		const std::filesystem::path& index_path,
		const std::filesystem::path& texture_path,
		std::vector<int>* out_material_indices)
	{
		std::ifstream vertex_file{ vertex_path, std::ios::out | std::ios::binary };
		std::ifstream index_file{ index_path, std::ios::out | std::ios::binary };
		std::ifstream texture_file{ texture_path, std::ios::out | std::ios::binary };

		// This map makes loading flexible to removal of unused textures since it associates old indices with new ones.
		std::unordered_map<uint32_t, uint32_t> texture_index_map{}; // Contains pairs of (old_index, new_index).
		texture_index_map[NULL_INDEX] = NULL_INDEX;

		// Load textures.
		for (auto& json_texture : j[jsonkey::TEXTURES])
		{
			uint32_t width{ json_texture[jsonkey::WIDTH] };
			uint32_t height{ json_texture[jsonkey::HEIGHT] };
			uint32_t channels{ json_texture[jsonkey::CHANNELS] };
			uint32_t old_index{ json_texture[jsonkey::TEXTURE_INDEX] };
			bool color_data{ json_texture[jsonkey::COLOR_DATA] };

			uint32_t byte_size{ width * height * channels };

			unsigned char* texture_data{ new unsigned char[byte_size] };
			texture_file.seekg((size_t)json_texture[jsonkey::TEXTURE_BYTE_OFFSET]);
			texture_file.read(reinterpret_cast<char*>(texture_data), byte_size);

			uint32_t new_index{ CreateTexture(texture_data, width, height, channels, color_data) };
			texture_index_map[old_index] = new_index;

			delete[] texture_data;
		}

		VkCommandBuffer cmd{ vulkan_util_.Begin() };

		// Load meshes.
		for (auto& json_mesh : j[jsonkey::MESHES])
		{
			Mesh* mesh{ new Mesh{} };
			meshes_.push_back(mesh);

			for (auto& json_geometry : json_mesh[jsonkey::GEOMETRIES])
			{
				auto& geometry{ mesh->geometries.emplace_back() };

				// Load vertices.
				geometry.vertices.resize(json_geometry[jsonkey::VERTEX_BYTE_SIZE] / sizeof(Vertex));
				vertex_file.seekg((size_t)json_geometry[jsonkey::VERTEX_BYTE_OFFSET]);
				vertex_file.read(reinterpret_cast<char*>(geometry.vertices.data()), json_geometry[jsonkey::VERTEX_BYTE_SIZE]);

				// Load indices.
				geometry.indices.resize(json_geometry[jsonkey::INDEX_BYTE_SIZE] / sizeof(decltype(Geometry::indices)::value_type));
				index_file.seekg((size_t)json_geometry[jsonkey::INDEX_BYTE_OFFSET]);
				index_file.read(reinterpret_cast<char*>(geometry.indices.data()), json_geometry[jsonkey::INDEX_BYTE_SIZE]);
			}

			UploadMeshToDevice(vulkan_util_, *mesh);
			rt_context_.QueueBlas(mesh);
		}

		// Load materials.
		for (auto& json_material : j[jsonkey::MATERIALS])
		{
			Material* material{ new Material{} };

			material->color = json_material[jsonkey::COLOR];
			material->metallic = json_material[jsonkey::METALLIC];
			material->roughness = json_material[jsonkey::ROUGHNESS];
			material->emission = json_material[jsonkey::EMISSION];
			material->ior = json_material[jsonkey::IOR];
			material->color_index = texture_index_map[json_material[jsonkey::COLOR_INDEX]];
			material->metallic_index = texture_index_map[json_material[jsonkey::METALLIC_INDEX]];
			material->roughness_index = texture_index_map[json_material[jsonkey::ROUGHNESS_INDEX]];
			material->emission_index = texture_index_map[json_material[jsonkey::EMISSION_INDEX]];
			material->normal_index = texture_index_map[json_material[jsonkey::NORMAL_INDEX]];

			materials_.push_back(material);
		}

		// Use less fine-grained memory barrier (instead of buffer memory barrier) since there's a buffer for each geometry,
		// and that would be a lot of pipeline barriers.
		PipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		// Load render objects.
		for (auto& json_ro : j[jsonkey::RENDER_OBJECTS])
		{
			std::vector<int> material_indices{ json_ro[jsonkey::MATERIAL_INDICES].get<std::vector<int>>() };
			CreateRenderObject(json_ro[jsonkey::MESH_INDEX], material_indices);
			out_material_indices->insert(out_material_indices->end(), material_indices.begin(), material_indices.end());
		}

		rt_context_.UpdateObjectBuffers(meshes_);
		rt_context_.UpdateMaterialBuffers(materials_, GetMaterialIndices());

		// Load mesh hash map.
		for (auto& json_hash_val : j[jsonkey::MESH_HASH_MAP])
		{
			mesh_hash_map_[json_hash_val[jsonkey::VERTEX_HASH]] = {
				json_hash_val[jsonkey::INDEX_HASH],
				json_hash_val[jsonkey::HASH_MAP_MESH_INDEX],
			};
		}
	}

	void VulkanRenderer::BuildTlasAndUpdateBlases()
	{
		// Delete the temporary buffers from the last frame.
		if (GetCurrentFrame().tlas)
		{
			vkDestroyAccelerationStructureKHR(context_.device, GetCurrentFrame().tlas->acceleration_structure, nullptr);
			allocator_.DestroyBufferResource(&GetCurrentFrame().tlas->buffer_resource);
		}
		rt_context_.DeleteTemporaryBuffers();

		// TODO: Update any BLASes that need to be updated here.

		GetCurrentFrame().tlas = rt_context_.QueueTlas(GetCurrentFrame().render_objects);
		VkCommandBuffer cmd{ vulkan_util_.Begin() };
		rt_context_.CmdBuildQueuedTlases(cmd);
		vulkan_util_.Submit();

		rt_context_.SetTlas(GetCurrentFrame().tlas->acceleration_structure);

	}

	Mesh* VulkanRenderer::GetMesh(uint32_t mesh_index)
	{
		return meshes_[mesh_index];
	}

	Mesh* VulkanRenderer::GetMesh(RenderObjectHandle render_object_handle)
	{
		RenderObject* render_object{ GetCurrentFrame().render_objects[render_object_handle] };
		return meshes_[render_object->mesh_idx];
	}

	const std::vector<int>& VulkanRenderer::GetMaterialIndices(RenderObjectHandle render_object_handle)
	{
		RenderObject* render_object{ GetCurrentFrame().render_objects[render_object_handle] };
		return render_object->material_indices;
	}

	uint32_t VulkanRenderer::GetCurrentFrameNumber() const
	{
		return current_frame_;
	}

	std::vector<Material*>& VulkanRenderer::GetMaterials()
	{
		return materials_;
	}

	void VulkanRenderer::UpdateMaterials()
	{
		rt_context_.UpdateMaterialBuffers(materials_, GetMaterialIndices());
	}

	std::vector<const std::vector<int>*> VulkanRenderer::GetMaterialIndices()
	{
		std::vector<const std::vector<int>*> material_indices{};
		for (const RenderObject* ro : GetCurrentFrame().render_objects)
		{
			if (ro) {
				material_indices.push_back(&ro->material_indices);
			}
		}
		return material_indices;
	}

	void VulkanRenderer::SetMaterialIndex(RenderObjectHandle render_object_handle, uint32_t geometry_index, int material_index)
	{
		for (FrameResources& frame_resource : frame_resources_) {
			frame_resource.render_objects[render_object_handle]->material_indices[geometry_index] = material_index;
		}
		UpdateMaterials();
	}

	Material* VulkanRenderer::MakeMaterialUnique(uint32_t material_index)
	{
		Material* new_material{ new Material{*materials_[material_index]} };
		materials_.push_back(new_material);
		UpdateMaterials();
		return new_material;
	}

	void VulkanRenderer::UpdateObjectBuffers()
	{
		rt_context_.UpdateObjectBuffers(meshes_);
	}

	std::vector<Rayhit> VulkanRenderer::CastRays(const std::vector<Raycast>& raycasts)
	{
		return rt_context_.CastRays(raycasts);
	}

	uint32_t VulkanRenderer::CreateTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels, bool color_data)
	{
		if (channels != 4) {
			logger::Error("Only textures with 4 channels are supported.");
		}

		ImageResource* texture_image{ new ImageResource{allocator_.CreateImageResource(
			{ width, height },
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			color_data ? COLOR_TEXTURE_FORMAT : NON_COLOR_TEXTURE_FORMAT)} };
		NameObject(context_.device, texture_image->image, "Texture_Image");
		NameObject(context_.device, texture_image->image_view, "Texture_Image_View");

		vulkan_util_.Begin();

		// Transition image to transfer dst layout.
		vulkan_util_.PipelineBarrier(
			texture_image->image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, VK_ACCESS_TRANSFER_WRITE_BIT);

		vulkan_util_.TransferImageToDevice(data, width * height * channels, *texture_image, width, height);

		// Transition image to shader read only, so the texture can be read in shaders.
		vulkan_util_.PipelineBarrier(
			texture_image->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		vulkan_util_.Submit();
		textures_.push_back(texture_image);

		rt_context_.UpdateTextureBuffers(textures_);

		return (uint32_t)(textures_.size() - 1);
	}

	uint32_t VulkanRenderer::GetTextureCount() const
	{
		return (uint32_t)textures_.size();
	}

	VkImageView VulkanRenderer::GetViewportImageView(uint32_t image_index)
	{
#ifdef EDITOR_ENABLED
		return editor_backend_.GetImGuiBackend().GetViewportImage().image_view;
#else
		return swapchain_.GetImageView(image_index);
#endif
	}

	VkImage VulkanRenderer::GetViewportImage(uint32_t image_index)
	{
#ifdef EDITOR_ENABLED
		return editor_backend_.GetImGuiBackend().GetViewportImage().image;
#else
		return swapchain_.GetImage(image_index);
#endif
	}

	VkImageView VulkanRenderer::GetViewportDepthImageView()
	{
#ifdef EDITOR_ENABLED
		return editor_backend_.GetImGuiBackend().GetViewportDepthImage().image_view;
#else
		return GetCurrentFrame().depth_image.image_view;
#endif
	}

	VkFormat VulkanRenderer::GetViewportImageFormat() const
	{
#ifdef EDITOR_ENABLED
		return editor_backend_.GetImGuiBackend().GetViewportImageFormat();
#else
		return swapchain_.GetImageFormat();
#endif
	}

	RenderObjectHandle VulkanRenderer::CreateRenderObject(uint32_t mesh_index, const std::vector<int>& material_indices)
	{
		for (auto& frame : frame_resources_)
		{
			RenderObject* render_object{ new RenderObject{
				.mesh_idx = mesh_index,
				.material_indices = material_indices,
				.visible = true,
				.uniform_buffer = {
					.transform = glm::mat4(1.0f),
				},
				.ubo_buffer_resource = allocator_.CreateBufferResource(sizeof(RenderObject::UniformBuffer),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
				.ubo_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(render_object_layout_resource_),
			} };
			NameObject(context_.device, render_object->ubo_buffer_resource.buffer, std::string{ "Render_Object_Buffer_" + std::to_string(render_object->mesh_idx) });

			render_object->ubo_descriptor_set_resource.LinkBufferToBinding(RENDER_OBJECT_UBO_BINDING, render_object->ubo_buffer_resource);
			frame.render_objects.push_back(render_object);
		}

		return (RenderObjectHandle)(frame_resources_[0].render_objects.size() - 1);
	}

	RenderObjectHandle VulkanRenderer::CreateRenderObjectFromMesh(Mesh* mesh, const std::vector<int>& material_indices)
	{
		uint32_t mesh_index{ (uint32_t)meshes_.size() };

		VkCommandBuffer cmd{ vulkan_util_.Begin() };
		UploadMeshToDevice(vulkan_util_, *mesh);
		PipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
		rt_context_.QueueBlas(mesh);
		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		meshes_.push_back(mesh);
		rt_context_.UpdateObjectBuffers(meshes_);
		return CreateRenderObject(mesh_index, material_indices);
	}

	RenderObjectHandle VulkanRenderer::CreateBlankRenderObject()
	{
		for (auto& frame : frame_resources_) {
			frame.render_objects.push_back(nullptr);
		}

		return (RenderObjectHandle)(frame_resources_[0].render_objects.size() - 1);
	}

	void VulkanRenderer::ReplaceRenderObject(RenderObjectHandle ro_target, Mesh* mesh)
	{
		// Upload mesh and build BLAS.
		VkCommandBuffer cmd{ vulkan_util_.Begin() };
		UploadMeshToDevice(vulkan_util_, *mesh);
		PipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

		rt_context_.QueueBlas(mesh);
		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		// Either replace old mesh if it exists or create new one.
		RenderObject* ro_ptr{ GetCurrentFrame().render_objects[ro_target] };
		bool visible{ true };
		std::vector<int> material_indices{ 0 };
		uint32_t mesh_index{};
		if (ro_ptr)
		{
			visible = ro_ptr->visible;
			material_indices = ro_ptr->material_indices;
			render_object_destroyer_.DestroyElement((uint32_t)ro_target);
			if (mesh)
			{
				mesh_index = render_object_destroyer_.PopVacantMeshIndex();

				if (mesh_index == NULL_INDEX)
				{
					mesh_index = (uint32_t)meshes_.size();
					meshes_.push_back(mesh);
				}
				else {
					meshes_[mesh_index] = mesh;
				}
			}
		}
		else if (mesh)
		{
			mesh_index = (uint32_t)meshes_.size();
			meshes_.push_back(mesh);
		}

		if (mesh) {
			rt_context_.UpdateObjectBuffers(meshes_);
		}

		// Replace old render object, referencing either new or replaced mesh.
		for (auto& frame : frame_resources_)
		{
			if (mesh)
			{
				RenderObject* render_object{ new RenderObject{
					.mesh_idx = mesh_index,
					.material_indices = material_indices,
					.visible = visible,
					.uniform_buffer = {
						.transform = glm::mat4(1.0f),
					},
					.ubo_buffer_resource = allocator_.CreateBufferResource(sizeof(RenderObject::UniformBuffer),
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
					.ubo_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(render_object_layout_resource_),
				} };
				NameObject(context_.device, render_object->ubo_buffer_resource.buffer, std::string{ "Replaced_Render_Object_Buffer_" + std::to_string(render_object->mesh_idx) });
				render_object->ubo_descriptor_set_resource.LinkBufferToBinding(RENDER_OBJECT_UBO_BINDING, render_object->ubo_buffer_resource);
				frame.render_objects[(size_t)ro_target] = render_object;
			}
			else {
				frame.render_objects[(size_t)ro_target] = nullptr;
			}
		}
	}

	void VulkanRenderer::SetRenderObjectTransform(RenderObjectHandle render_object_handle, const glm::mat4& transform)
	{
		// This is called every frame by Pumpkin so we only update for the current frame resources.
		RenderObject* render_object{ GetCurrentFrame().render_objects[render_object_handle] };
		if (!render_object) {
			return;
		}

		render_object->uniform_buffer.transform = transform;

		void* data{};
		vkMapMemory(context_.device, *render_object->ubo_buffer_resource.memory, render_object->ubo_buffer_resource.offset, render_object->ubo_buffer_resource.size, 0, &data);
		memcpy(data, &render_object->uniform_buffer, sizeof(RenderObject::UniformBuffer));
		vkUnmapMemory(context_.device, *render_object->ubo_buffer_resource.memory);
	}

	void VulkanRenderer::SetRenderObjectVisible(RenderObjectHandle render_object_handle, bool visible)
	{
		for (auto& frame : frame_resources_) {
			frame.render_objects[render_object_handle]->visible = visible;
		}
	}

	void VulkanRenderer::SetCameraMatrix(const glm::mat4& view, const glm::mat4& projection)
	{
		auto& cam_buffer{ GetCurrentFrame().camera_ubo_buffer };
		auto& cam_ubo{ GetCurrentFrame().camera_ubo };

		cam_ubo.projection_view = projection * view;
		rt_context_.SetCameraMatrices(view, projection);

		void* data{};
		vkMapMemory(context_.device, *cam_buffer.memory, cam_buffer.offset, cam_buffer.size, 0, &data);
		memcpy(data, &cam_ubo, sizeof(FrameResources::RasterizationCameraUBO));
		vkUnmapMemory(context_.device, *cam_buffer.memory);
	}

	void VulkanRenderer::InitializeDescriptorSetLayouts()
	{
		VkDescriptorSetLayoutBinding ubo_binding{
			.binding = RENDER_OBJECT_UBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> camera_bindings{
			ubo_binding,
		};

		camera_layout_resource_ = descriptor_allocator_.CreateDescriptorSetLayoutResource(camera_bindings, 0);
		NameObject(context_.device, camera_layout_resource_.layout, "Camera_Descriptor_Set_Layout");

		std::vector<VkDescriptorSetLayoutBinding> render_object_bindings{
			ubo_binding,
		};

		render_object_layout_resource_ = descriptor_allocator_.CreateDescriptorSetLayoutResource(render_object_bindings, 0);
		NameObject(context_.device, render_object_layout_resource_.layout, "Render_Object_Descriptor_Set_Layout");

		VkDescriptorSetLayoutBinding raster_image_binding{
			.binding = COMPOSITE_RASTER_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding rt_image_binding{
			.binding = COMPOSITE_RT_IMAGE_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> composite_bindings{
			raster_image_binding,
			rt_image_binding,
		};

		composite_layout_resource_ = descriptor_allocator_.CreateDescriptorSetLayoutResource(composite_bindings, 0);
		NameObject(context_.device, composite_layout_resource_.layout, "Composite_Descriptor_Set_Layout");
	}

	void VulkanRenderer::InitializeFrameResources()
	{
		InitializeCommandBuffers();
		InitializeSyncObjects();
		InitializeDescriptorResources();

		//InitializeRayTraceImages();
		InitializeDepthImages();
	}

	void VulkanRenderer::InitializeCommandBuffers()
	{
		VkCommandPoolCreateInfo pool_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = context_.GetGraphicsQueueFamilyIndex(),
		};

		VkResult result{ vkCreateCommandPool(context_.device, &pool_info, nullptr, &command_pool_) };
		CheckResult(result, "Failed to create command pool.");
		NameObject(context_.device, command_pool_, "Main_Command_Pool");

		VkCommandBufferAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool_,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		for (FrameResources& resource : frame_resources_)
		{
			VkCommandBuffer* cmd_buffer{ &resource.command_buffer };
			VkResult result{ vkAllocateCommandBuffers(context_.device, &allocate_info, cmd_buffer) };
			CheckResult(result, "Failed to allocate command buffer.");
		}
	}

	void VulkanRenderer::InitializeSyncObjects()
	{
		// Start in signaled state since frame resources are initially not in use.
		VkFenceCreateInfo fence_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		VkSemaphoreCreateInfo semaphore_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.flags = 0 // Reserved.
		};

		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			VkResult result{ vkCreateFence(context_.device, &fence_info, nullptr, &resource.render_done_fence) };
			CheckResult(result, "Failed to create fence.");
			NameObject(context_.device, resource.render_done_fence, "Render_Done_Fence_" + std::to_string(i));

			result = vkCreateSemaphore(context_.device, &semaphore_info, nullptr, &resource.image_acquired_semaphore);
			CheckResult(result, "Failed to create semaphore.");
			NameObject(context_.device, resource.image_acquired_semaphore, "Image_Acquired_Semaphore_" + std::to_string(i));

			result = vkCreateSemaphore(context_.device, &semaphore_info, nullptr, &resource.render_done_semaphore);
			CheckResult(result, "Failed to create semaphore.");
			NameObject(context_.device, resource.render_done_semaphore, "Render_Done_Semaphore_" + std::to_string(i));

			++i;
		}
	}

	void VulkanRenderer::InitializeDescriptorResources()
	{
		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			// Camera resources.
			resource.camera_ubo_buffer = allocator_.CreateBufferResource(sizeof(FrameResources::RasterizationCameraUBO),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			NameObject(context_.device, resource.camera_ubo_buffer.buffer, std::string{ "Camera_UBO_Buffer_" + std::to_string(i) });

			resource.camera_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(camera_layout_resource_);
			resource.camera_descriptor_set_resource.LinkBufferToBinding(CAMERA_UBO_BINDING, resource.camera_ubo_buffer);

			// Composite image resource. Don't link to binding yet, since we do this when window is resized.
			resource.composite_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(composite_layout_resource_);
			++i;
		}
	}

	/*
	void VulkanRenderer::InitializeRayTraceImages()
	{
		std::array<ImageResource, FRAMES_IN_FLIGHT> rt_images;

		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			// Destroy image if it exists.
			allocator_.DestroyImageResource(&resource.rt_image);

			resource.rt_image = allocator_.CreateImageResource(
				GetViewportExtent(),
				VK_IMAGE_USAGE_STORAGE_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				swapchain_.GetImageFormat());

			rt_images[i] = resource.rt_image;
			++i;
		}

		rt_context_.SetRenderImages(GetViewportExtent(), rt_images);
	}
	*/

	void VulkanRenderer::UpdateImages()
	{
		rt_context_.SetRenderImages(GetViewportExtent(), editor_backend_.GetImGuiBackend().GetRayTraceImages());

		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			resource.composite_descriptor_set_resource.LinkImageToBinding(COMPOSITE_RASTER_BINDING, editor_backend_.GetImGuiBackend().GetRasterImages()[i], VK_IMAGE_LAYOUT_GENERAL);
			resource.composite_descriptor_set_resource.LinkImageToBinding(COMPOSITE_RT_IMAGE_BINDING, editor_backend_.GetImGuiBackend().GetRayTraceImages()[i], VK_IMAGE_LAYOUT_GENERAL);
			++i;
		}

	}

	void VulkanRenderer::InitializeDepthImages()
	{
#ifndef EDITOR_ENABLED

		for (FrameResources& resource : frame_resources_)
		{
			resource.depth_image = allocator_.CreateImageResource(
				GetViewportExtent(),
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				GetDepthImageFormat());
		}

		// Maybe TODO: Transition image with image barrier for being a depth image?
#endif
	}

	std::vector<int> VulkanRenderer::LoadMeshesAndMaterialsGLTF(tinygltf::Model& model, std::vector<std::string>* out_material_names)
	{
		std::vector<int> duplicate_indices{};
		duplicate_indices.reserve(model.meshes.size());

		VkCommandBuffer cmd{ vulkan_util_.Begin() };

		// Load meshes.
		for (tinygltf::Mesh& tinygltf_mesh : model.meshes)
		{
			Mesh* mesh{ new Mesh{} };
			mesh->geometries.resize(tinygltf_mesh.primitives.size());

			uint64_t vertex_hash{ LoadVerticesGLTF(model, tinygltf_mesh, mesh) };
			uint64_t index_hash{ LoadIndicesGLTF(model, tinygltf_mesh, mesh) };
			CalculateTangents(mesh);

			// Check if this mesh has been loaded before, and only add to meshes_ if it hasn't
			auto it{ mesh_hash_map_.find(vertex_hash) }; // This is a nested pair (vertex_hash, (index_hash, mesh_index)).
			if (it != mesh_hash_map_.end() && it->second.first == index_hash)
			{
				duplicate_indices.push_back(it->second.second); // Save the index into meshes_ where this mesh has been loaded before.
				delete mesh;
			}
			else
			{
				mesh_hash_map_[vertex_hash] = std::pair<uint64_t, uint32_t>{ index_hash, (uint32_t)meshes_.size() };
				UploadMeshToDevice(vulkan_util_, *mesh);
				rt_context_.QueueBlas(mesh);
				duplicate_indices.push_back(-1); // -1 indicates this mesh has not been loaded before.
				meshes_.push_back(mesh);
			}
		}

		// Load materials.
		for (tinygltf::Material& tinygltf_material : model.materials)
		{
			if (out_material_names) {
				out_material_names->push_back(tinygltf_material.name);
			}

			materials_.push_back(new Material{ default_material });
		}

		// If gltf file doesn't include materials, just create a default material for it.
		if (model.materials.empty())
		{
			if (out_material_names) {
				out_material_names->push_back("DefaultMaterial");
			}

			materials_.push_back(new Material{ default_material });
		}

		PipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		rt_context_.UpdateObjectBuffers(meshes_);
		// We won't update the material buffers until after we've created render objects since we need material indices.

		return duplicate_indices;
	}

	uint32_t VulkanRenderer::InvokeParticleGenShader(RenderObjectHandle ro_target)
	{
		if (materials_.empty()) {
			materials_.push_back(new Material{ default_material });
		}

		particle_context_.SetTargetRenderObject(ro_target);
		return particle_context_.InvokeParticleGenShader();
	}

	uint32_t VulkanRenderer::GenerateTestParticle(RenderObjectHandle ro_target)
	{
		if (materials_.empty()) {
			materials_.push_back(new Material{ default_material });
		}

		particle_context_.SetTargetRenderObject(ro_target);
		particle_context_.GenerateTestParticle();
		return 1;
	}

	void VulkanRenderer::SetParticleGenShader(uint32_t shader_idx, uint32_t custom_ubo_size)
	{
		particle_context_.SetParticleGenShader(shader_idx, custom_ubo_size);
	}

	void VulkanRenderer::UpdateParticleGenShaderCustomUBO(const std::vector<std::byte>& custom_ubo)
	{
		particle_context_.UpdateParticleGenShaderCustomUBO(custom_ubo);
	}

	void VulkanRenderer::PlayParticleSimulation()
	{
		particle_context_.EnablePhysicsUpdate();
	}

	void VulkanRenderer::PauseParticleSimulation()
	{
		particle_context_.DisablePhysicsUpdate();
	}

	void VulkanRenderer::ResetParticleSimulation()
	{
		particle_context_.ResetParticles();
	}

	bool VulkanRenderer::GetParticleSimulationEnabled() const
	{
		return particle_context_.GetPhysicsUpdateEnabled();
	}

	bool VulkanRenderer::GetParticleSimulationEmpty() const
	{
		return particle_context_.GetParticlesEmpty();
	}

	void VulkanRenderer::ParticleUpdate(float delta_time)
	{
		particle_context_.PhysicsUpdate(delta_time);
	}

	void VulkanRenderer::ImportShader(const std::filesystem::path& spirv_path)
	{
		ComputePipeline* compute_pipeline{ new ComputePipeline{} };

		std::vector<DescriptorSetLayoutResource> compute_layouts{
			particle_context_.GetParticleGenLayoutResource(),
		};

		compute_pipeline->Initialize(&context_, compute_layouts, {}, spirv_path);
		NameObject(context_.device, compute_pipeline->pipeline, "Compute_Pipeline");
		NameObject(context_.device, compute_pipeline->layout, "Compute_Pipeline_Layout");

		user_compute_shaders_.push_back(compute_pipeline);
	}

	void VulkanRenderer::HostRenderWork()
	{
		ZoneScoped;
		render_object_destroyer_.FrameUpdate();
	}

	void VulkanRenderer::ComputeWork()
	{

	}

	void VulkanRenderer::UploadMeshToDevice(VulkanUtil& vulkan_util, Mesh& mesh)
	{
		for (Geometry& geometry : mesh.geometries)
		{
			std::string mesh_name{ NameMesh(mesh.geometries) };

			geometry.vertices_resource = allocator_.CreateBufferResource(geometry.vertices.size() * sizeof(Vertex),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vulkan_util.TransferBufferToDevice(geometry.vertices, geometry.vertices_resource);
			NameObject(context_.device, geometry.vertices_resource.buffer, std::string{ mesh_name + "_Vertex_Buffer" });

			geometry.indices_resource = allocator_.CreateBufferResource(geometry.indices.size() * sizeof(decltype(Geometry::indices)::value_type),
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vulkan_util.TransferBufferToDevice(geometry.indices, geometry.indices_resource);
			NameObject(context_.device, geometry.indices_resource.buffer, std::string{ mesh_name + "_Index_Buffer" });
		}
	}

	void VulkanRenderer::DestroyRenderObject(RenderObject* ro_ptr, bool last_resource)
	{
		if (!ro_ptr) {
			return;
		}

		// If all the other render objects using this mesh have been destroyed then
		// delete the mesh referenced by the render object.
		if (last_resource)
		{
			uint32_t mesh_idx{ ro_ptr->mesh_idx };
			DestroyMesh(mesh_idx);
		}

		// Delete this frame's render object.
		allocator_.DestroyBufferResource(&ro_ptr->ubo_buffer_resource);
		descriptor_allocator_.FreeDescriptorSet(&ro_ptr->ubo_descriptor_set_resource.descriptor_set);
		delete ro_ptr;
	}

	void VulkanRenderer::DestroyMesh(uint32_t mesh_idx)
	{
		Mesh* mesh{ meshes_[mesh_idx] };
		if (!mesh) {
			return;
		}

		vkDestroyAccelerationStructureKHR(context_.device, mesh->blas.acceleration_structure, nullptr);
		allocator_.DestroyBufferResource(&mesh->blas.buffer_resource);

		for (Geometry& geometry : mesh->geometries)
		{
			allocator_.DestroyBufferResource(&geometry.vertices_resource);
			allocator_.DestroyBufferResource(&geometry.indices_resource);
		}

		delete mesh;
		meshes_[mesh_idx] = nullptr;
	}

	bool VulkanRenderer::GetViewportMinimized() const
	{
		// If we're using the editor and the viewport is minimized, we skip rendering to the 3D viewport.
#ifdef EDITOR_ENABLED
		return !editor_backend_.GetImGuiBackend().GetViewportVisible();
#else
		return false;
#endif
	}

	uint32_t VulkanRenderer::MeshCount() const
	{
		return (uint32_t)meshes_.size();
	}
}
