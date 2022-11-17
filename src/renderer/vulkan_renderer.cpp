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

#include "vulkan_util.h"
#include "logger.h"
#include "context.h"
#include "mesh.h"
#include "descriptor_set.h"

namespace jsonkey {
	const std::string RENDER_OBJECTS{ "render_objects" };
	// Render object members.
	const std::string MESH_INDEX{ "mesh_index" };
	const std::string VERTEX_TYPE{ "vertex_type" };
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

	const std::string MESH_HASH_MAP{ "mesh_hash_map" };
	// Mesh hash map members.
	const std::string VERTEX_HASH{ "vertex_hash" };
	const std::string INDEX_HASH{ "index_hash" };
	const std::string HASH_MAP_MESH_INDEX{ "hash_map_mesh_index" };
	// End mesh hash map members.
}

namespace renderer
{
	// Camera descriptor set.
	static constexpr uint32_t CAMERA_UBO_SET{ 0 };
	static constexpr uint32_t CAMERA_UBO_BINDING{ 0 };
	// Render object descriptor set.
	static constexpr uint32_t RENDER_OBJECT_UBO_SET{ 1 };
	static constexpr uint32_t RENDER_OBJECT_UBO_BINDING{ 0 };

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
		InitializeRayTracing();

#ifdef EDITOR_ENABLED
		imgui_backend_.Initialize(this);
#endif
	}

#ifdef EDITOR_ENABLED
	void VulkanRenderer::SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks)
	{
		imgui_backend_.SetImGuiCallbacks(imgui_callbacks);
	}

	void VulkanRenderer::SetImGuiViewportSize(const Extent& extent)
	{
		imgui_backend_.SetViewportSize(extent);
	}
#endif

	void VulkanRenderer::CleanUp()
	{
		VkResult result{ vkDeviceWaitIdle(context_.device) };
		CheckResult(result, "Error waiting for device to idle.");

#ifdef EDITOR_ENABLED
		imgui_backend_.CleanUp();
#endif

		rt_context_.CleanUp();

		for (FrameResources& frame : frame_resources_)
		{
			vkDestroyFence(context_.device, frame.render_done_fence, nullptr);
			vkDestroySemaphore(context_.device, frame.image_acquired_semaphore, nullptr);
			vkDestroySemaphore(context_.device, frame.render_done_semaphore, nullptr);

			for (RenderObject& render_object : frame.render_objects) {
				allocator_.DestroyBufferResource(&render_object.ubo_buffer_resource);
			}
			allocator_.DestroyBufferResource(&frame.camera_ubo_buffer);

			if (frame.tlas)
			{
				vkDestroyAccelerationStructureKHR(context_.device, frame.tlas->acceleration_structure, nullptr);
				allocator_.DestroyBufferResource(&frame.tlas->buffer_resource);
			}
		}

		descriptor_allocator_.DestroyDescriptorSetLayoutResource(&render_object_layout_resource_);
		descriptor_allocator_.DestroyDescriptorSetLayoutResource(&camera_layout_resource_);

		for (Mesh* mesh : meshes_) {
			DestroyMesh(mesh);
		}
		meshes_.clear();

		// Destroying command pool frees all command buffers allocated from it.
		vkDestroyCommandPool(context_.device, command_pool_, nullptr);

		vulkan_util_.CleanUp();
		allocator_.CleanUp();
		graphics_pipeline_.CleanUp();
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
	}

	void VulkanRenderer::InitializePipelines()
	{
		std::vector<DescriptorSetLayoutResource> set_layouts{
			camera_layout_resource_,
			render_object_layout_resource_,
		};

		graphics_pipeline_.Initialize(&context_, &swapchain_, set_layouts, GetDepthImageFormat());
	}

	void VulkanRenderer::InitializeRayTracing()
	{
		rt_context_.Initialize(&context_, &allocator_);
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
		// Request image from the swapchain, one second timeout.
		// This is also where vsync happens according to vkguide, but for me it happens at present.
		uint32_t image_index{ swapchain_.AcquireNextImage(GetCurrentFrame().image_acquired_semaphore) };

		// Now that we are sure that the commands finished executing, we can safely
		// reset the command buffer to begin recording again.
		VkResult result{ vkResetCommandBuffer(GetCurrentFrame().command_buffer, 0) };
		CheckResult(result, "Error resetting command buffer.");

#ifdef EDITOR_ENABLED
		imgui_backend_.DrawGui();
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

		NextFrame();
	}

	void VulkanRenderer::Draw(VkCommandBuffer cmd, uint32_t image_index)
	{
		Extent viewport_extents{ GetViewportExtent() };
		VkClearColorValue clear_color{ 0.0f, 0.0f, 0.2f, 1.0f };

		VkRenderingAttachmentInfo color_attachment_info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = GetViewportImageView(image_index),
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

		// If we're using the editor and the viewport is minimized, we skip rendering to the 3D viewport.
#ifdef EDITOR_ENABLED
		bool minimized{ !imgui_backend_.GetViewportVisible() };
#else
		constexpr bool minimized{ false };
#endif

#ifdef EDITOR_ENABLED
		if (!minimized)
		{
			// If we're using the editor's image we need to transition it to be a color attachment.
			PipelineBarrier(
				cmd, imgui_backend_.GetViewportImage().image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			);
		}
#endif

		if (!minimized)
		{
			// First render pass. Render 3D Viewport.
			vkCmdBeginRendering(cmd, &rendering_info);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.layout, CAMERA_UBO_SET, 1, &GetCurrentFrame().camera_descriptor_set_resource.descriptor_set, 0, nullptr);

			VkDeviceSize zero_offset{ 0 };

			for (auto& render_obj : GetCurrentFrame().render_objects)
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_.layout, RENDER_OBJECT_UBO_SET, 1, &render_obj.ubo_descriptor_set_resource.descriptor_set, 0, nullptr);

				for (auto& geometry : meshes_[render_obj.mesh_idx]->geometries)
				{
					vkCmdBindVertexBuffers(cmd, 0, 1, &geometry.vertices_resource.buffer, &zero_offset);
					vkCmdBindIndexBuffer(cmd, geometry.indices_resource.buffer, 0, VK_INDEX_TYPE_UINT16);
					vkCmdDrawIndexed(cmd, (uint32_t)geometry.indices.size(), 1, 0, 0, 0);
				}
			}

			vkCmdEndRendering(cmd);
		}

#ifdef EDITOR_ENABLED
		if (!minimized)
		{
			// Pipeline barrier to make sure previous rendering finishes before fragment shader.
			// Also transitions image layout to be read from shader.
			PipelineBarrier(
				cmd, imgui_backend_.GetViewportImage().image,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT
			);
		}

		// Second render pass. Render editor GUI if the editor is enabled.
		Extent window_extent{ context_.GetWindowExtent() };
		color_attachment_info.imageView = swapchain_.GetImageView(image_index);
		rendering_info.renderArea.extent = { window_extent.width, window_extent.height };
		rendering_info.pDepthAttachment = nullptr;

		vkCmdBeginRendering(cmd, &rendering_info);
		imgui_backend_.RecordCommandBuffer(cmd);
		vkCmdEndRendering(cmd);
#endif
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
		bool minimized{ !imgui_backend_.GetViewportVisible() };
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

		TransitionSwapImageForRender(cmd, image_index);
		Draw(cmd, image_index);
		TransitionSwapImageForPresent(cmd, image_index);

		result = vkEndCommandBuffer(GetCurrentFrame().command_buffer);
		CheckResult(result, "Failed to end command buffer.");
	}

	void VulkanRenderer::TransitionSwapImageForRender(VkCommandBuffer cmd, uint32_t image_index)
	{
		// TODO: Use vulkan_util.cpp stateless pipeline barrier here and in below function.

		// Transition image to optimal layout for rendering to.
		VkImageMemoryBarrier image_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain_.GetImage(image_index),
			.subresourceRange = {
			  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			  .baseMipLevel = 0,
			  .levelCount = 1,
			  .baseArrayLayer = 0,
			  .layerCount = 1,
			}
		};

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
			0,
			0,
			nullptr,
			0,
			nullptr,
			1, // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
	}

	void VulkanRenderer::TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index)
	{
		// Transition image to optimal format for presentation.
		VkImageMemoryBarrier image_memory_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain_.GetImage(image_index),
			.subresourceRange = {
			  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			  .baseMipLevel = 0,
			  .levelCount = 1,
			  .baseArrayLayer = 0,
			  .layerCount = 1,
			}
		};

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
			VK_DEPENDENCY_BY_REGION_BIT,
			0,
			nullptr,
			0,
			nullptr,
			1, // imageMemoryBarrierCount
			&image_memory_barrier // pImageMemoryBarriers
		);
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
		return imgui_backend_.GetViewportExtent();
#else
		return context_.GetWindowExtent();
#endif
	}

	void VulkanRenderer::DumpRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path) const
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
		for (const RenderObject& ro : GetCurrentFrame().render_objects)
		{
			j[jsonkey::RENDER_OBJECTS] += {
				{ jsonkey::MESH_INDEX, ro.mesh_idx },
				{ jsonkey::VERTEX_TYPE, ro.vertex_type },
			};
		}

		uint32_t vertex_byte_offest{ 0 };
		uint32_t index_byte_offset{ 0 };

		std::ofstream vertex_file{ vertex_path, std::ios::out | std::ios::binary };
		std::ofstream index_file{ index_path, std::ios::out | std::ios::binary };

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
					{ jsonkey::INDEX_BYTE_SIZE, geometry.indices.size() * sizeof(uint16_t) },
				};

				vertex_file.write(reinterpret_cast<const char*>(geometry.vertices.data()), geometry.vertices.size() * sizeof(Vertex));
				index_file.write(reinterpret_cast<const char*>(geometry.indices.data()), geometry.indices.size() * sizeof(uint16_t));

				vertex_byte_offest += geometry.vertices.size() * sizeof(Vertex);
				index_byte_offset += geometry.indices.size() * sizeof(uint16_t);
			}

			j[jsonkey::MESHES] += json_mesh;
		}

		vertex_file.close();
		index_file.close();
	}

	void VulkanRenderer::LoadRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path)
	{
		std::ifstream vertex_file{ vertex_path, std::ios::out | std::ios::binary };
		std::ifstream index_file{ index_path, std::ios::out | std::ios::binary };

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
				geometry.indices.resize(json_geometry[jsonkey::INDEX_BYTE_SIZE] / sizeof(uint16_t));
				index_file.seekg((size_t)json_geometry[jsonkey::INDEX_BYTE_OFFSET]);
				index_file.read(reinterpret_cast<char*>(geometry.indices.data()), json_geometry[jsonkey::INDEX_BYTE_SIZE]);
			}

			UploadMeshToDevice(vulkan_util_, *mesh);
			rt_context_.QueueBlas(mesh);
		}

		// Use less fine-grained memory barrier (instead of buffer memory barrier) since there's a buffer for each geometry,
		// and that would be a lot of pipeline barriers.
		PipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);

		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		// Load render objects.
		for (auto& json_ro : j[jsonkey::RENDER_OBJECTS]) {
			CreateRenderObject(json_ro[jsonkey::MESH_INDEX]);
		}

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

		std::vector<VkAccelerationStructureInstanceKHR> vk_instances{};

		for (const RenderObject& render_object : GetCurrentFrame().render_objects) {
			vk_instances.push_back(RenderObjectToVulkanInstance(render_object));
		}

		GetCurrentFrame().tlas = rt_context_.QueueTlas(vk_instances);

		// TODO: Update any BLASes that need to be updated here.

		if (!vk_instances.empty())
		{
			VkCommandBuffer cmd{ vulkan_util_.Begin() };
			rt_context_.CmdBuildQueuedTlases(cmd);
			vulkan_util_.Submit();
		}
	}

	VkImageView VulkanRenderer::GetViewportImageView(uint32_t image_index)
	{
#ifdef EDITOR_ENABLED
		return imgui_backend_.GetViewportImage().image_view;
#else
		return swapchain_.GetImageView(image_index);
#endif
	}

	VkImageView VulkanRenderer::GetViewportDepthImageView()
	{
#ifdef EDITOR_ENABLED
		return imgui_backend_.GetViewportDepthImage().image_view;
#else
		return GetCurrentFrame().depth_image.image_view;
#endif
	}

	RenderObjectHandle VulkanRenderer::CreateRenderObject(uint32_t mesh_index)
	{
		for (auto& frame : frame_resources_)
		{
			RenderObject render_object{
				.mesh_idx = mesh_index,
				.vertex_type = VertexType::POSITION_NORMAL_COORD,
				.uniform_buffer = {
					.transform = glm::mat4(1.0f),
				},
				.ubo_buffer_resource = allocator_.CreateBufferResource(sizeof(RenderObject::UniformBuffer),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
				.ubo_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(render_object_layout_resource_),
			};
			NameObject(context_.device, render_object.ubo_buffer_resource.buffer, std::string{ "Render_Object_Buffer_" + std::to_string(render_object.mesh_idx) });

			render_object.ubo_descriptor_set_resource.LinkBufferToBinding(RENDER_OBJECT_UBO_BINDING, render_object.ubo_buffer_resource);
			frame.render_objects.push_back(render_object);
		}

		return (RenderObjectHandle)(frame_resources_[0].render_objects.size() - 1);
	}

	void VulkanRenderer::SetRenderObjectTransform(RenderObjectHandle render_object_handle, const glm::mat4& transform)
	{
		RenderObject& render_object{ GetCurrentFrame().render_objects[render_object_handle] };
		render_object.uniform_buffer.transform = transform;

		void* data{};
		vkMapMemory(context_.device, *render_object.ubo_buffer_resource.memory, render_object.ubo_buffer_resource.offset, render_object.ubo_buffer_resource.size, 0, &data);
		memcpy(data, &render_object.uniform_buffer, sizeof(RenderObject::UniformBuffer));
		vkUnmapMemory(context_.device, *render_object.ubo_buffer_resource.memory);
	}

	void VulkanRenderer::SetCameraMatrix(const glm::mat4& projection_view)
	{
		auto& cam_buffer{ GetCurrentFrame().camera_ubo_buffer };
		auto& cam_ubo{ GetCurrentFrame().camera_ubo };

		cam_ubo.projection_view = projection_view;

		void* data{};
		vkMapMemory(context_.device, *cam_buffer.memory, cam_buffer.offset, cam_buffer.size, 0, &data);
		memcpy(data, &cam_ubo, sizeof(FrameResources::CameraUBO));
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

		camera_layout_resource_ = descriptor_allocator_.CreateDescriptorSetLayoutResource(camera_bindings);

		std::vector<VkDescriptorSetLayoutBinding> render_object_bindings{
			ubo_binding,
		};

		render_object_layout_resource_ = descriptor_allocator_.CreateDescriptorSetLayoutResource(render_object_bindings);
	}

	void VulkanRenderer::InitializeFrameResources()
	{
		InitializeCommandBuffers();
		InitializeSyncObjects();
		InitializeCameraResources();
#ifndef EDITOR_ENABLED
		InitializeDepthImages();
#endif
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

	void VulkanRenderer::InitializeCameraResources()
	{
		uint32_t i{ 0 };
		for (FrameResources& resource : frame_resources_)
		{
			resource.camera_ubo_buffer = allocator_.CreateBufferResource(sizeof(FrameResources::CameraUBO),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			NameObject(context_.device, resource.camera_ubo_buffer.buffer, std::string{ "Camera_UBO_Buffer_" + std::to_string(i) });

			resource.camera_descriptor_set_resource = descriptor_allocator_.CreateDescriptorSetResource(camera_layout_resource_);
			resource.camera_descriptor_set_resource.LinkBufferToBinding(CAMERA_UBO_BINDING, resource.camera_ubo_buffer);
			++i;
		}
	}

	void VulkanRenderer::InitializeDepthImages()
	{
		for (FrameResources& resource : frame_resources_)
		{
			resource.depth_image = allocator_.CreateImageResource(
				GetViewportExtent(),
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				GetDepthImageFormat()
			);
		}

		// Maybe todo: Transition image with image barrier for being a depth image?
	}

	std::vector<int> VulkanRenderer::LoadMeshesGLTF(tinygltf::Model& model)
	{
		std::vector<int> duplicate_indices{};
		duplicate_indices.reserve(model.meshes.size());

		VkCommandBuffer cmd{ vulkan_util_.Begin() };

		for (tinygltf::Mesh& tinygltf_mesh : model.meshes)
		{
			Mesh* mesh{ new Mesh{} };
			meshes_.push_back(mesh);
			mesh->geometries.resize(tinygltf_mesh.primitives.size());

			uint64_t vertex_hash{ LoadVerticesGLTF(model, tinygltf_mesh, mesh) };
			uint64_t index_hash{ LoadIndicesGLTF(model, tinygltf_mesh, mesh) };

			// Check if this mesh has been loaded before, and only add to meshes_ if it hasn't
			auto it{ mesh_hash_map_.find(vertex_hash) }; // This is a nested pair (vertex_hash, (index_hash, mesh_index)).
			if (it != mesh_hash_map_.end() && it->second.first == index_hash) {
				duplicate_indices.push_back(it->second.second); // Save the index into meshes_ where this mesh has been loaded before.
			}
			else
			{
				mesh_hash_map_[vertex_hash] = std::pair<uint64_t, uint32_t>{ index_hash, (uint32_t)meshes_.size() };
				UploadMeshToDevice(vulkan_util_, *mesh);
				rt_context_.QueueBlas(mesh);
				duplicate_indices.push_back(-1); // -1 indicates this mesh has not been loaded before.
			}
		}

		rt_context_.CmdBuildQueuedBlases(cmd);
		vulkan_util_.Submit();

		return duplicate_indices;
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
			NameObject(context_.device, geometry.vertices_resource.buffer, std::string{ mesh_name + "_Vertex_Buffer"});

			geometry.indices_resource = allocator_.CreateBufferResource(geometry.indices.size() * sizeof(uint16_t),
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vulkan_util.TransferBufferToDevice(geometry.indices, geometry.indices_resource);
			NameObject(context_.device, geometry.indices_resource.buffer, std::string{ mesh_name + "_Index_Buffer" });
		}
	}

	void VulkanRenderer::DestroyMesh(Mesh* mesh)
	{
		vkDestroyAccelerationStructureKHR(context_.device, mesh->blas.acceleration_structure, nullptr);
		allocator_.DestroyBufferResource(&mesh->blas.buffer_resource);

		for (Geometry& geometry : mesh->geometries)
		{
			allocator_.DestroyBufferResource(&geometry.vertices_resource);
			allocator_.DestroyBufferResource(&geometry.indices_resource);
		}

		delete mesh;
		mesh = nullptr;
	}

	VkAccelerationStructureInstanceKHR VulkanRenderer::RenderObjectToVulkanInstance(const RenderObject& render_object) const
	{
		VkAccelerationStructureDeviceAddressInfoKHR device_address_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
			.accelerationStructure = meshes_[render_object.mesh_idx]->blas.acceleration_structure,
		};

		return VkAccelerationStructureInstanceKHR{
			.transform = ToVulkanTransformMatrix(render_object.uniform_buffer.transform),
			.instanceCustomIndex = 0,
			.mask = 0xFF,
			.instanceShaderBindingTableRecordOffset = 0,
			.flags = 0,
			.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(context_.device, &device_address_info),
		};
	}

	uint32_t VulkanRenderer::MeshCount() const
	{
		return (uint32_t)meshes_.size();
	}
}
