#pragma once

#include <vector>
#include <filesystem>
#include <unordered_map>
#include <array>
#include "volk.h"

#include "context.h"
#include "memory_allocator.h"
#include "descriptor_set.h"
#include "renderer_constants.h"
#include "vulkan_util.h"
#include "descriptor_set.h"
#include "renderer_types.h"
#include "render_object.h"
#include "mesh.h"

namespace renderer
{
	// Device addresses of object buffers cast to uint64_t to match closest-hit shader.
	// There is one object buffer per geometry so the index will be given by custom_index + geometry_index.
	struct ObjectBuffers
	{
		uint64_t vertices;
		uint64_t indices;
	};

	struct ShaderBindingTable
	{
		VkStridedDeviceAddressRegionKHR raygen_sbt_address;
		VkStridedDeviceAddressRegionKHR miss_sbt_address;
		VkStridedDeviceAddressRegionKHR hit_sbt_address;
		VkStridedDeviceAddressRegionKHR callable_sbt_address;
		BufferResource raygen_sbt;
		BufferResource miss_sbt;
		BufferResource hit_sbt;
		// TODO: BufferResource for callable shaders.
	};

	struct Raycast
	{
		glm::vec4 origin;
		glm::vec4 direction;
	};

	struct Rayhit
	{
		uint32_t instance_index;
		glm::vec3 position;
	};

	std::vector<VkAccelerationStructureBuildRangeInfoKHR> GetGeometryBuidRanges(const std::vector<Geometry>& geometries);

	// Utility for building the shader binding table.
	// Step 1: Initialize and add all the shader groups.
	// Step 2: Create the ray tracing pipeline using shader stages and groups from GetShaderStages() and GetGroups() respectively.
	// Step 3: Pass the newly built pipeline to Build(...) to create the SBT buffers.
	// Step 4: Pass the device addresses from ShaderBindingTable to vkCmdTraceRaysKHR(...).
	class ShaderBindingTableBuilder
	{
	public:
		void Initialize(Context* context, Allocator* allocator, VulkanUtil* vulkan_util, VkPhysicalDeviceRayTracingPipelinePropertiesKHR* rt_pipeline_properties);

		void CleanUp();

		void SetRaygenShader(const std::filesystem::path& spirv_path);

		void AddMissShader(const std::filesystem::path& spirv_path);

		void AddHitGroup(
			const std::filesystem::path& closest_hit,
			const std::filesystem::path& any_hit,
			const std::filesystem::path& intersection
		);

		const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStages() const;

		// Get the groups in the order (raygen_group, miss_groups, hit_groups).
		const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& GetGroups();

		ShaderBindingTable Build(VkPipeline pipeline);

	private:
		// Adds hit shader or reuse previously added one if it's been added before.
		uint32_t AddHitShader(const std::filesystem::path& shader, VkShaderStageFlagBits shader_stage_flag);

		std::vector<VkPipelineShaderStageCreateInfo> shader_stages_{};
		VkRayTracingShaderGroupCreateInfoKHR raygen_group_{};
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> miss_groups_{};
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> hit_groups_{};
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> concatenated_groups_{};
		bool concatenated_groups_dirty_{ false };

		// Pairs of (shader path, index into shader_stages_).
		// Needed to avoid duplicate shaders if shaders are reused across hit groups.
		std::unordered_map<std::filesystem::path, uint32_t> hit_shader_index_map_{};

		Context* context_{};
		Allocator* allocator_{};
		VulkanUtil* vulkan_util_{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR* rt_pipeline_properties_{};
	};

	class VulkanRenderer;

	class RayTracingContext
	{
	public:
		void Initialize(Context* context,
			VulkanRenderer* renderer,
			Allocator* allocator,
			DescriptorAllocator* descriptor_allocator,
			VulkanUtil* vulkan_util);

		void CleanUp();

		// Saves address of BLAS without build data that will be populated after CmdBuildQueuedBlases(...) is called.
		// The build data will not be present in the BLAS buffer until after CmdBuildQueuedBlases(...) is called and the queue is submitted.
		void QueueBlas(
			AccelerationStructure* blas,
			std::vector<VkAccelerationStructureGeometryKHR>&& vk_geometries,
			std::vector<VkAccelerationStructureBuildRangeInfoKHR>&& build_ranges);

		// Call this if mesh does have CPU side mesh data.
		void QueueBlas(Mesh* mesh);

		// Call this if mesh does not have CPU side mesh data.
		// Will mutate mesh_info, so will be invalid after this function is called.
		void QueueBlas(Mesh* mesh, MeshBlasInfo& mesh_info);

		// Returns empty TLAS without build data that will be populated after CmdBuildQueuedTlases(...) is called.
		// The build data will not be present in the TLAS buffer until after CmdBuildQueuedTlases(...) is called and the queue is submitted.
		AccelerationStructure* QueueTlas(const std::vector<RenderObject*>& render_objects);

		// Creates the BLAS objects populating the empty BLASes saved from QueueBlas(...) and writes the build
		// commands into the command buffer.
		// Includes pipeline barriers for BLAS buffers.
		void CmdBuildQueuedBlases(VkCommandBuffer cmd);

		// Creates the TLAS objects populating the empty TLASes returned from QueueTlas(...) and writes the build
		// commands into the command buffer.
		// Includes pipeline barriers for TLAS buffers.
		void CmdBuildQueuedTlases(VkCommandBuffer cmd);

		VkAccelerationStructureInstanceKHR RenderObjectToVulkanInstance(const RenderObject& render_object) const;

		void Render(VkCommandBuffer cmd);

		void DeleteTemporaryBuffers();

		void SetCameraMatrices(const glm::mat4& view, const glm::mat4& projection);

		void SetTlas(VkAccelerationStructureKHR tlas);

		void SetRenderImages(const Extent& render_extent, const std::array<ImageResource, FRAMES_IN_FLIGHT>& render_images);

		// Updates the buffers containing vertex data to be accessed in closest-hit shaders. This could maybe be handled automatically
		// when CmdBuildQueuedBlases(...) is called, but that would require keeping track of previously built meshes.
		void UpdateObjectBuffers(const std::vector<Mesh*>& meshes);

		// The indices argument is an array of material indices for each Vulkan instance. Each of these arrays contains one material index for each geometry
		// contained by the BLAS referenced by the instance. This vector of vectors will be converted to two GPU buffers, one containing the concatenated indices
		// and the other containing device addresses into those indices so that each Vulkan instance will have an associated device address pointing to an
		// of indices for all of its geometries.
		void UpdateMaterialBuffers(const std::vector<Material*>& materials, const std::vector<const std::vector<int>*>& indices);

		void UpdateTextureBuffers(const std::vector<ImageResource*>& textures);

		// Cast rays for none rendering purposes, eg for clicking objects in the scene.
		// Returns vector of Rayhits of same size as input raycasts.
		std::vector<Rayhit> CastRays(const std::vector<Raycast>& raycasts);

	private:
		struct FrameResources;

		VkAccelerationStructureGeometryKHR PumpkinTriGeometryToVulkanGeometry(const Geometry& pmk_geometry, uint32_t max_vertex) const;

		VkAccelerationStructureGeometryKHR PumpkinTriGeometryToVulkanGeometry(const Geometry& pmk_geometry) const;

		std::vector<VkAccelerationStructureGeometryKHR> PumpkinTriGeometriesToVulkanGeometries(const std::vector<Geometry>& pmk_geometries, const std::vector<uint32_t>& max_vertices) const;

		std::vector<VkAccelerationStructureGeometryKHR> PumpkinSingleGeometryToVulkanGeometries(const Geometry& pmk_geometry, const std::vector<uint32_t>& max_vertices) const;

		std::vector<VkAccelerationStructureGeometryKHR> PumpkinTriGeometriesToVulkanGeometries(const std::vector<Geometry>& pmk_geometries) const;

		void CreateAccelerationStructure(VkDeviceSize acceleration_structure_size, bool top_level, AccelerationStructure* out_blas) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(
			const VkAccelerationStructureBuildGeometryInfoKHR& build_info,
			const std::vector<uint32_t>& primitive_counts) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(
			const VkAccelerationStructureBuildGeometryInfoKHR& build_info,
			uint32_t instance_count) const;

		void UploadInstancesToDevice(VkCommandBuffer cmd, const std::vector<VkAccelerationStructureInstanceKHR>& instances);

		void CreateRtPipelineAndShaderBindingTable();

		void CreateRtPipelineLayout();

		void CreateRaycastPipelineAndShaderBindingTable();

		void CreateRaycastPipelineLayout();

		void CreateDescriptorSets();

		FrameResources& GetCurrentFrame();

		void DestroyFrameResources();

		void CreateAndLinkRaycastBuffers();

		struct QueuedBlasBuildInfo
		{
			// Allocate BLAS when it's added to queue so we can return that to caller to associate with the mesh, even though it won't yet be built.
			AccelerationStructure* blas;
			// The actual geometry data needed for the BLAS.
			const std::vector<VkAccelerationStructureGeometryKHR> vk_geometries;
			std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges;
		};

		struct QueuedTlasBuildInfo
		{
			// Allocate TLAS when it's added to queue so we can return that to caller to associate with the mesh, even though it won't yet be built.
			AccelerationStructure* tlas;
			// The acceleration structure instances needed for the TLAS.
			std::vector<VkAccelerationStructureInstanceKHR> instances;
		};

		struct RayTraceCameraUBO
		{
			glm::mat4 view_inverse;
			glm::mat4 proj_inverse;
		};

		struct FrameResources
		{
			BufferResource camera_ubo_buffer;
			DescriptorSetResource frame_descriptor_set_resource_{};
			BufferResource instance_buffer_{}; // Store so we can delete it after the TLAS is built.
			std::vector<BufferResource> scratch_buffers_{};                           // Store these so we can delete them after the acceleration structures are built.
			std::vector<BufferResource> staging_buffers_{};                           // Store these so we can delete them after the acceleration structures are built.
		};

		std::vector<QueuedBlasBuildInfo> queued_blas_build_infos_{};              // Info needed to build the BLASes when CmdBuildQueuedBlases(...) is called.
		std::vector<QueuedTlasBuildInfo> queued_tlas_build_infos_{};              // Info needed to build the TLASes when CmdBuildQueuedTlases(...) is called.
		BufferResource object_buffers_buffer_{};                                  // Buffer containing device addresses to mesh data for each object in the scene. Not in FrameResources since it's rarely updated.
		BufferResource materials_resource_{};                                     // Buffer containing all ray tracing material data.
		BufferResource material_indices_resource_{};                              // Buffer containing concatenated ray tracing material indices for all geometries for all render objects.
		BufferResource material_index_addresses_resource_{};                      // Buffer containing device addresses into material_indices_resource_. Each address acts as a buffer of materials for geometries of a render object.
		std::unordered_map<AccelerationStructure*, uint32_t> custom_index_map_{}; // Map of (BLAS, custom_index) so when TLAS is queued we have access to custom indices.

		VkPipeline rt_pipeline_{};
		VkPipelineLayout rt_pipeline_layout_{};
		DescriptorSetLayoutResource frame_descriptor_set_layout_resource_{};
		DescriptorSetLayoutResource persistent_descriptor_set_layout_resource_{};
		DescriptorSetResource persistent_descriptor_set_resource_{};              // Descriptor set for resources that do not change each frame.
		ShaderBindingTable rt_shader_binding_table_{};
		Extent render_extent_{};

		VkPipeline raycast_pipeline_{};
		VkPipelineLayout raycast_pipeline_layout_{};
		DescriptorSetLayoutResource raycast_descriptor_set_layout_resource_{};
		DescriptorSetResource raycast_descriptor_set_resource_{};
		BufferResource raycasts_buffer_{};
		BufferResource rayhits_buffer_{};
		ShaderBindingTable raycast_shader_binding_table_{};

		VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties_{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties_{};
		Context* context_{};
		VulkanRenderer* renderer_{};
		Allocator* allocator_{};
		DescriptorAllocator* descriptor_allocator_{};
		VulkanUtil* vulkan_util_{};

		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
	};
}
