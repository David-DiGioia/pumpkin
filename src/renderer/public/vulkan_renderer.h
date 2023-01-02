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
#include "render_object.h"

namespace renderer
{
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
		// If out_material_names is not null, then loaded material names will be written into it.
		std::vector<int> LoadMeshesAndMaterialsGLTF(tinygltf::Model& model, std::vector<std::string>* out_material_names);

		uint32_t MeshCount() const;

		// Create a render object with the buffer resource and descriptors already associated
		// with the render object data.
		RenderObjectHandle CreateRenderObject(uint32_t mesh_index, const std::vector<int>& material_indices);

		void SetRenderObjectTransform(RenderObjectHandle render_object_handle, const glm::mat4& transform);

		void SetCameraMatrix(const glm::mat4& view, const glm::mat4& projection);

		Extent GetViewportExtent();

		void DumpRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path) const;

		void LoadRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path, std::vector<int>* out_material_indices);

		void BuildTlasAndUpdateBlases();

		Mesh* GetMesh(uint32_t mesh_index);

		Mesh* GetMesh(RenderObjectHandle render_object_handle);

		std::vector<int>& GetMaterialIndices(RenderObjectHandle render_object_handle) ;

		uint32_t GetCurrentFrameNumber() const;

		std::vector<Material*>& GetMaterials();

		void UpdateMaterials();

		Material* MakeMaterialUnique(uint32_t material_index);

		void UpdateObjectBuffers();

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

		//void InitializeRayTraceImages();

		void SetRayTraceImages(const std::array<ImageResource, FRAMES_IN_FLIGHT>& rt_images);

		void InitializeDepthImages();

		void InitializeDescriptorSetLayouts();

		void UploadMeshToDevice(VulkanUtil& vulkan_util, Mesh& mesh);

		void DestroyMesh(Mesh* mesh);

		struct FrameResources
		{
			std::vector<RenderObject> render_objects;

			struct RasterizationCameraUBO
			{
				glm::mat4 projection_view;
			} camera_ubo;
			BufferResource camera_ubo_buffer;
			DescriptorSetResource camera_descriptor_set_resource;

			VkCommandBuffer command_buffer;
			VkFence render_done_fence;
			VkSemaphore image_acquired_semaphore;
			VkSemaphore render_done_semaphore;

			AccelerationStructure* tlas;

			// Only used when EDITOR_ENABLED is not defined, since ImguiBackend has ownership over these otherwise.
			// This is not in the Swapchain class since we only need frame-in-flight number depth images.
			ImageResource depth_image;
			//ImageResource rt_image;
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

		std::vector<Mesh*> meshes_{};        // All meshes referenced by render objects.
		std::vector<Material*> materials_{}; // All materials referenced by geometries. Buffer resource for materials is in RayTracingContext.
		std::unordered_map<uint64_t, std::pair<uint64_t, uint32_t>> mesh_hash_map_{}; // To prevent duplicating vertex data when loading same file multiple times. (vertex_hash, (index_hash, mesh_idx)).
		DescriptorSetLayoutResource camera_layout_resource_{};
		DescriptorSetLayoutResource render_object_layout_resource_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
	};
}
