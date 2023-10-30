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
#include "editor_backend.h"
#include "renderer_types.h"
#include "ray_tracing.h"
#include "render_object.h"
#include "renderer_constants.h"
#include "particles.h"

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

		// Invoke user-defined particle gen shader. Generated render object will replace ro_target.
		void InvokeParticleGenShader(RenderObjectHandle ro_target);

		void SetParticleGenShader(uint32_t shader_idx, const std::vector<std::byte>& custom_ubo_buffer);

		void PlayParticleSimulation();

		void PauseParticleSimulation();

		void ResetParticleSimulation();

		bool GetParticleSimulationEnabled() const;

		bool GetParticleSimulationEmpty() const;

		// Called at regular intervals to advance particles by one time step.
		void ParticleUpdate(float delta_time);

		void ImportShader(const std::filesystem::path& spirv_path);

		// GPU work for renderer to do each frame.
		void ComputeWork();

		uint32_t MeshCount() const;

		// Create a render object with the buffer resource and descriptors already associated
		// with the render object data.
		RenderObjectHandle CreateRenderObject(uint32_t mesh_index, const std::vector<int>& material_indices);

		RenderObjectHandle CreateRenderObjectFromMesh(Mesh* mesh, const std::vector<int>& material_indices);

		// Get a blank render object to be used as a render object target when generating a mesh later.
		RenderObjectHandle CreateBlankRenderObject();

		// Doesn't take mesh index since it reuses the mesh index that the previous render object used.
		void ReplaceRenderObject(RenderObjectHandle ro_target, Mesh* mesh, const std::vector<int>& material_indices);

		void SetRenderObjectTransform(RenderObjectHandle render_object_handle, const glm::mat4& transform);

		void SetRenderObjectVisible(RenderObjectHandle render_object_handle, bool visible);

		void SetCameraMatrix(const glm::mat4& view, const glm::mat4& projection);

		Extent GetViewportExtent();

		void DumpRenderData(
			nlohmann::json& j,
			const std::filesystem::path& vertex_path,
			const std::filesystem::path& index_path,
			const std::filesystem::path& texture_path);

		void LoadRenderData(
			nlohmann::json& j,
			const std::filesystem::path& vertex_path,
			const std::filesystem::path& index_path,
			const std::filesystem::path& texture_path,
			std::vector<int>* out_material_indices);

		void BuildTlasAndUpdateBlases();

		Mesh* GetMesh(uint32_t mesh_index);

		Mesh* GetMesh(RenderObjectHandle render_object_handle);

		const std::vector<int>& GetMaterialIndices(RenderObjectHandle render_object_handle);

		uint32_t GetCurrentFrameNumber() const;

		std::vector<Material*>& GetMaterials();

		void UpdateMaterials();

		std::vector<const std::vector<int>*> GetMaterialIndices();

		void SetMaterialIndex(RenderObjectHandle render_object_handle, uint32_t geometry_index, int material_index);

		Material* MakeMaterialUnique(uint32_t material_index);

		void UpdateObjectBuffers();

		std::vector<Rayhit> CastRays(const std::vector<Raycast>& raycasts);

		// Returns texture index into textures_ vector.
		uint32_t CreateTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels, bool color_data);

		uint32_t GetTextureCount() const;

#ifdef EDITOR_ENABLED
		void SetImGuiCallbacks(const ImGuiCallbacks& imgui_callbacks);

		void SetImGuiViewportSize(const Extent& extent);

		void AddOutlineSet(const std::vector<RenderObjectHandle>& selection_set, const glm::vec4& color);

		void SetParticleOverlayEnabled(bool render_grid, bool render_nodes, bool rasterize_particles);

		void SetParticleOverlay(RenderObjectHandle render_object);

		void ClearOutlineSets();

		void SetParticleColorMode(uint32_t color_mode);

		void SetNodeColorMode(uint32_t color_mode);
#endif

	private:
		struct FrameResources;

		void Draw(VkCommandBuffer cmd, uint32_t image_index);

		void RasterRenderPass(VkCommandBuffer cmd);

		void EditorGuiRenderPass(VkCommandBuffer cmd, uint32_t image_index);

		void CompositeRenderPass(VkCommandBuffer cmd, uint32_t image_index);

		void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionImagesForRender(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index);

		void NextFrame();

		FrameResources& GetCurrentFrame();

		const FrameResources& GetCurrentFrame() const;

		// We don't return an image resource here since we make the viewports separate from the VkImages made by the swapchain.
		VkImageView GetViewportImageView(uint32_t image_index);

		// We don't return an image resource here since swapchain creates VkImages directly.
		VkImage GetViewportImage(uint32_t image_index);

		VkImageView GetViewportDepthImageView();

		VkFormat GetViewportImageFormat() const;

		VkFormat GetDepthImageFormat() const;

		void InitializePipelines();

		void InitializeRayTracing();

		void InitializeFrameResources();

		void InitializeCommandBuffers();

		void InitializeSyncObjects();

		void InitializeDescriptorResources();

		//void InitializeRayTraceImages();

		void UpdateImages();

		void InitializeDepthImages();

		void InitializeDescriptorSetLayouts();

		void UploadMeshToDevice(VulkanUtil& vulkan_util, Mesh& mesh);

		void DestroyRenderObject(RenderObjectHandle render_object_handle);

		void DestroyMesh(Mesh* mesh);

		bool GetViewportMinimized() const;

		struct FrameResources
		{
			std::vector<RenderObject*> render_objects;

			struct RasterizationCameraUBO
			{
				glm::mat4 projection_view;
			} camera_ubo;
			BufferResource camera_ubo_buffer;
			DescriptorSetResource camera_descriptor_set_resource;

			DescriptorSetResource composite_descriptor_set_resource;

			VkCommandBuffer command_buffer;
			VkFence render_done_fence;
			VkSemaphore image_acquired_semaphore;
			VkSemaphore render_done_semaphore;

			AccelerationStructure* tlas;

#ifndef EDITOR_ENABLED
			// Only used when EDITOR_ENABLED is not defined, since ImguiBackend has ownership over these otherwise.
			// This is not in the Swapchain class since we only need frame-in-flight number depth images.
			ImageResource depth_image;
			//ImageResource rt_image;
#endif
		};

		friend class EditorBackend;
		friend class ImGuiBackend;
		friend class ParticleContext;
#ifdef EDITOR_ENABLED
		EditorBackend editor_backend_{};
#endif

		Context context_{};
		Swapchain swapchain_{};
		GraphicsPipeline raster_pipeline_{};
		GraphicsPipeline composite_pipeline_{};
		VkCommandPool command_pool_{};
		Allocator allocator_{};
		DescriptorAllocator descriptor_allocator_{};
		VulkanUtil vulkan_util_{};
		RayTracingContext rt_context_{};
		ParticleContext particle_context_{};

		std::vector<Mesh*> meshes_{};                          // All meshes referenced by render objects.
		std::vector<Material*> materials_{};                   // All materials referenced by geometries. Buffer resource for materials is in RayTracingContext.
		std::vector<ImageResource*> textures_{};               // All textures referenced by materials.
		std::vector<ComputePipeline*> user_compute_shaders_{}; // All user-defined compute shaders.
		std::unordered_map<uint64_t, std::pair<uint64_t, uint32_t>> mesh_hash_map_{}; // To prevent duplicating vertex data when loading same file multiple times. (vertex_hash, (index_hash, mesh_idx)).
		DescriptorSetLayoutResource camera_layout_resource_{};
		DescriptorSetLayoutResource render_object_layout_resource_{};
		DescriptorSetLayoutResource composite_layout_resource_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
	};
}
