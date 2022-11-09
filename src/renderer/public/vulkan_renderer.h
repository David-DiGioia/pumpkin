#pragma once

#include <array>
#include <vector>
#include <filesystem>
#include "volk.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "nlohmann/json.hpp"

#include "context.h"
#include "swapchain.h"
#include "pipeline.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "vulkan_util.h"
#include "descriptor_set.h"
#include "imgui_backend.h"
#include "renderer_types.h"
#include "ray_tracing.h"

namespace renderer
{
	struct RenderObject
	{
		uint32_t mesh_idx;
		VertexType vertex_type;

		struct UniformBuffer
		{
			glm::mat4 transform;
		} uniform_buffer;

		BufferResource ubo_buffer_resource;
		DescriptorSetResource ubo_descriptor_set_resource;
	};

	class VulkanRenderer
	{
	public:
		void Initialize(GLFWwindow* window);

		void CleanUp();

		// Waits for the last frame with the same index, since the frame resources
		// will be occupied until it finishes rendering.
		//
		// Do all CPU work that mutates render objects between WaitForLastFrame() and Render().
		void WaitForLastFrame();

		void Render();

		void WindowResized();

		// Returns list where ith entry corresponds to model.meshes[i], -1 if it's a new mesh, or index into meshes_ if it's been loaded before.
		std::vector<int> LoadMeshesGLTF(tinygltf::Model& model);

		uint32_t MeshCount() const;

		// Create a render object with the buffer resource and descriptors already associated
		// with the render object data.
		RenderObjectHandle CreateRenderObject(uint32_t mesh_index);

		void SetRenderObjectTransform(RenderObjectHandle render_object_handle, const glm::mat4& transform);

		void SetCameraMatrix(const glm::mat4& projection_view);

		Extent GetViewportExtent();

		void DumpRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path) const;

		void LoadRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path);

		void BuildTlasAndUpdateBlases();

#ifdef EDITOR_ENABLED
		void SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks);

		void SetImGuiViewportSize(const Extent& extent);
#endif

	private:
		struct FrameResources;

		void Draw(VkCommandBuffer cmd, uint32_t image_index);

		void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForRender(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index);

		void NextFrame();

		FrameResources& GetCurrentFrame();

		const FrameResources& GetCurrentFrame() const;

		VkImageView GetViewportImageView(uint32_t image_index);

		VkImageView GetViewportDepthImageView();

		VkFormat GetDepthImageFormat() const;

		void InitializePipelines();

		void InitializeRayTracing();

		void InitializeFrameResources();

		void InitializeCommandBuffers();

		void InitializeSyncObjects();

		void InitializeCameraResources();

		void InitializeDepthImages();

		void InitializeDescriptorSetLayouts();

		void UploadMeshToDevice(Mesh& mesh);

		VkAccelerationStructureInstanceKHR RenderObjectToVulkanInstance(const RenderObject& render_object) const;

		struct FrameResources
		{
			std::vector<RenderObject> render_objects{};

			struct CameraUBO
			{
				glm::mat4 projection_view;
			} camera_ubo;

			BufferResource camera_ubo_buffer;
			DescriptorSetResource camera_descriptor_set_resource{};

			VkCommandBuffer command_buffer;
			VkFence render_done_fence;
			VkSemaphore image_acquired_semaphore;
			VkSemaphore render_done_semaphore;

			// Only used when EDITOR_ENABLED is not defined.
			// This is not in the Swapchain class since we only need frame-in-flight number depth images.
			ImageResource depth_image;
		};

		// Even though these are only used when EDITOR_ENABLED is defined, we don't change the structs
		// between the editor enabled/disabled projects or we get runtime errors maybe stemming from
		// breaking the one definition rule?
		friend class ImGuiBackend;
		ImGuiBackend imgui_backend_{};

		Context context_{};
		Swapchain swapchain_{};
		GraphicsPipeline graphics_pipeline_{};
		VkCommandPool command_pool_{};
		Allocator allocator_{};
		DescriptorAllocator descriptor_allocator_{};
		VulkanUtil vulkan_util_{};
		RayTracingContext rt_context_{};

		std::vector<Mesh> meshes_{}; // All meshes referenced by render objects.
		std::unordered_map<uint64_t, std::pair<uint64_t, uint32_t>> mesh_hash_map_{}; // To prevent duplicating vertex data when loading same file multiple times. (vertex_hash, (index_hash, mesh_idx)).
		DescriptorSetLayoutResource camera_layout_resource_{};
		DescriptorSetLayoutResource render_object_layout_resource_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
	};
}
