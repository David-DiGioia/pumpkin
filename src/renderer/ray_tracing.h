#pragma once

#include <vector>
#include "volk.h"

#include "context.h"
#include "memory_allocator.h"

namespace renderer
{
	struct Mesh;
	struct Geometry;

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR acceleration_structure;
		BufferResource buffer_resource;
	};

	class RayTracingContext
	{
	public:
		void Initialize(Context* context, Allocator* alloc);

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

		void DeleteTemporaryBuffers();

	private:
		VkAccelerationStructureGeometryKHR PumpkinTriGeometryToVulkanGeometry(const Geometry& pmk_geometry) const;

		void CreateAccelerationStructure(VkDeviceSize acceleration_structure_size, bool top_level, AccelerationStructure* out_blas) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const std::vector<Geometry>& geometries) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, uint32_t instance_count) const;

		VkDeviceAddress DeviceAddress(VkBuffer buffer) const;

		BufferResource UploadInstancesToDevice(VkCommandBuffer cmd, const std::vector<VkAccelerationStructureInstanceKHR>& instances);

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

		VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties_{};
		Context* context_{};
		Allocator* allocator_{};
	};
}
