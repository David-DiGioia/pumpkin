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
#include "mesh.h"

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

		VkFormat GetViewportImageFormat() const;

		ImageResource& GetViewportImage();

		ImageResource& GetViewportDepthImage();

		ImageResource& GetRasterImage();

		std::array<ImageResource, FRAMES_IN_FLIGHT> GetRasterImages();

		std::array<ImageResource, FRAMES_IN_FLIGHT> GetRayTraceImages();

		void TransitionImagesForRender(VkCommandBuffer cmd);

		void TransitionColorPassesForSampling(VkCommandBuffer cmd);

		void TransitionFinalImageForSampling(VkCommandBuffer cmd);

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
			ImageResource raster_image;
			ImageResource rt_image;
			ImageResource final_image;
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
		void Initialize(
			const std::vector<Vertex>& cube_vertices,
			const std::vector<uint32_t>& cube_indices,
			Context* context,
			VulkanRenderer* renderer);

		void CleanUp();

		void InitializeDescriptorSetLayouts();

		ImGuiBackend& GetImGuiBackend();

		const ImGuiBackend& GetImGuiBackend() const;

		void SetViewportSize(const Extent& extent);

		void EditorRenderPasses(VkCommandBuffer cmd);

		void AddOutlineSet(std::vector<uint32_t>&& selection_set, const glm::vec4& color);

		void ClearOutlineSets();

		void SetRenderObjectInfo(float chunk_width, uint32_t render_object_index);

		void SetGridEnabled(bool enabled);

		void SetRasterParticlesEnabled(bool enabled);

		// Whether to use raster particle's depth buffer to occlude the particle overlay.
		void SetParticleDepthEnabled(bool enabled);

		void SetRigidBodyOverlayEnabled(bool enabled);

		void SetXPBDDebugParticleInstances(const std::vector<XPBDDebugParticleInstance>& particle_instances);

		void SetDebugRbVoxelInstances(const std::vector<RigidBodyDebugVoxelInstance>& rb_voxel_instances);

		void SetParticleColorMode(uint32_t color_mode);

		void SetParticleColorModeMaxValue(float max_value);

	private:
		struct OutlineObjects
		{
			std::vector<uint32_t> render_object_indices;
			glm::vec4 color;
		};

		struct FrameResources
		{
			// Outlines.
			ImageResource mask_image;
			DescriptorSetResource outline_set_resource;

			// Grid.
			ImageResource particle_depth;
		};

		struct ColorModePushConstant
		{
			uint32_t particle_color_mode{};
			float max_value{};
		};

		struct PhysicsDebugInfo
		{
			// Vertices of particle sized cube used for raster particles.
			BufferResource cube_vertices; // Of type Vertex.
			BufferResource cube_indices;

			// Vertices for line used to draw some visualizations of nodes.
			BufferResource line_vertices;

			// Use FRAMES_IN_FLIGHT geometry buffers so even if we rewrite the geometry every frame
			// we won't write to resources in use. But don't make part of FrameResources since we
			// don't rewrite geometry every frame.
			BufferResource particle_instances[FRAMES_IN_FLIGHT]; // Of type MPMDebugParticleInstance.
			uint32_t particle_idx;                               // Current index into particle_instances.

			BufferResource rb_voxel_instances[FRAMES_IN_FLIGHT]; // Of type RigidBodyDebugVoxelInstance.
			uint32_t rigid_body_idx;                                     // Current index into particle_instances.

			uint32_t render_object_index;
			uint32_t particle_instance_count;
			uint32_t cube_vertex_count;
			uint32_t cube_index_count;
			uint32_t rb_voxel_instance_count;

			uint32_t grid_vertex_count;
			BufferResource grid_vertices; // Of type Vertex.

			ColorModePushConstant particle_push_constant;
		};

		FrameResources& GetCurrentFrame();

		void InitializeFrameResources();

		void CreateFrameImages();

		void DestroyFrameImages();

		void RenderOutlines(VkCommandBuffer cmd);

		void MaskRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set);

		void OutlineRenderPass(VkCommandBuffer cmd, const OutlineObjects& outline_set);

		void RenderSelectedChunkOverlay(VkCommandBuffer cmd);

		// Render particle depth buffer and optionally color.
		void ParticleRasterRenderPass(VkCommandBuffer cmd);

		void GridRenderPass(VkCommandBuffer cmd);

		void RigidBodyRenderPass(VkCommandBuffer cmd);

		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};

		Context* context_{};
		VulkanRenderer* renderer_{};
		ImGuiBackend imgui_backend_{};
		GraphicsPipeline mask_pipeline_{};
		GraphicsPipeline outline_pipeline_{};
		GraphicsPipeline particle_raster_pipeline_{};
		GraphicsPipeline grid_pipeline_{};
		GraphicsPipeline rigid_body_line_pipeline_{};
		std::vector<OutlineObjects> outline_objects_{}; // Editor render pass will draw outlines around these sets of render objects.
		DescriptorSetLayoutResource outline_layout_resource_{};
		PhysicsDebugInfo physics_debug_{};
		bool grid_enabled_{};
		bool raster_particles_enabled_{};
		bool use_particle_depth_{};
		bool rigid_bodies_enabled_{};
	};
}
