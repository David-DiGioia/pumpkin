#pragma once

#include <vector>
#include <filesystem>
#include <unordered_map>
#include "volk.h"

#include "context.h"
#include "memory_allocator.h"
#include "descriptor_set.h"
#include "renderer_constants.h"
#include "vulkan_util.h"
#include "descriptor_set.h"

namespace renderer
{
	struct Mesh;
	struct Geometry;

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR acceleration_structure;
		BufferResource buffer_resource;
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

	class RayTracingContext
	{
	public:
		void Initialize(Context* context, Allocator* allocator, DescriptorAllocator* descriptor_allocator, VulkanUtil* vulkan_util);

		void CleanUp();

		// Saves address of mesh BLAS without build data that will be populated after CmdBuildQueuedBlases(...) is called.
		// The build data will not be present in the BLAS buffer until after CmdBuildQueuedBlases(...) is called and the queue is submitted.
		void QueueBlas(Mesh* mesh);

		// Returns empty TLAS without build data that will be populated after CmdBuildQueuedTlases(...) is called.
		// The build data will not be present in the TLAS buffer until after CmdBuildQueuedTlases(...) is called and the queue is submitted.
		AccelerationStructure* QueueTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances);

		// Creates the BLAS objects populating the empty BLASes returned from QueueBlas(...) and writes the build
		// commands into the command buffer.
		// Includes pipeline barriers for BLAS buffers.
		void CmdBuildQueuedBlases(VkCommandBuffer cmd);

		// Creates the TLAS objects populating the empty TLASes returned from QueueTlas(...) and writes the build
		// commands into the command buffer.
		// Includes pipeline barriers for TLAS buffers.
		void CmdBuildQueuedTlases(VkCommandBuffer cmd);

		void CmdTraceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth);

		void DeleteTemporaryBuffers();

	private:
		VkAccelerationStructureGeometryKHR PumpkinTriGeometryToVulkanGeometry(const Geometry& pmk_geometry) const;

		void CreateAccelerationStructure(VkDeviceSize acceleration_structure_size, bool top_level, AccelerationStructure* out_blas) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const std::vector<Geometry>& geometries) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, uint32_t instance_count) const;

		BufferResource UploadInstancesToDevice(VkCommandBuffer cmd, const std::vector<VkAccelerationStructureInstanceKHR>& instances);

		void CreatePipelineAndShaderBindingTable();

		void CreatePipelineLayout();

		struct QueuedBlasBuildInfo
		{
			// Allocate BLAS when it's added to queue so we can return that to caller to associate with the mesh, even though it won't yet be built.
			AccelerationStructure* blas;
			// The actual geometry data needed for the BLAS.
			const std::vector<Geometry>* geometries;
		};

		struct QueuedTlasBuildInfo
		{
			// Allocate TLAS when it's added to queue so we can return that to caller to associate with the mesh, even though it won't yet be built.
			AccelerationStructure* tlas;
			// The acceleration structure instances needed for the TLAS.
			const std::vector<VkAccelerationStructureInstanceKHR>* instances;
		};

		std::vector<QueuedBlasBuildInfo> queued_blas_build_infos_{}; // Info needed to build the BLASes when CmdBuildQueuedBlases(...) is called.
		std::vector<QueuedTlasBuildInfo> queued_tlas_build_infos_{}; // Info needed to build the TLASes when CmdBuildQueuedTlases(...) is called.
		std::vector<BufferResource> scratch_buffers_{}; // Store these so we can delete them after the acceleration structures are built.
		std::vector<BufferResource> staging_buffers_{}; // Store these so we can delete them after the acceleration structures are built.
		BufferResource instance_buffer_{}; // Store so we can delete it after the TLAS is built.

		VkPipeline rt_pipeline_{};
		VkPipelineLayout rt_pipeline_layout_{};
		DescriptorSetLayoutResource rt_set_layout_resource_{};
		ShaderBindingTable shader_binding_table_{};

		VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties_{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties_{};
		Context* context_{};
		Allocator* allocator_{};
		DescriptorAllocator* descriptor_allocator_{};
		VulkanUtil* vulkan_util_{};
	};
}
