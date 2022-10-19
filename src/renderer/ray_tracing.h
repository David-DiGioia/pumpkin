#pragma once

#include <vector>
#include "volk.h"

#include "context.h"
#include "memory_allocator.h"
#include "mesh.h"

namespace renderer
{
	struct Blas
	{
		VkAccelerationStructureKHR blas;
		BufferResource buffer_resource;
	};

	class RayTracingContext
	{
	public:
		void Initialize(Context* context, Allocator* alloc);

		void CleanUp();

		void AddBlas(const Mesh& mesh);

		void BuildBlases(VkCommandBuffer cmd);

	private:
		VkAccelerationStructureGeometryKHR PumpkinGeometryToVulkanGeometry(const Geometry& pmk_geometry) const;

		void CreateBlas(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const VkAccelerationStructureBuildSizesInfoKHR& build_sizes, Blas* out_blas) const;

		VkAccelerationStructureBuildSizesInfoKHR GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const std::vector<Geometry>& geometries) const;

		VkDeviceAddress DeviceAddress(VkBuffer buffer) const;

		std::vector<const Mesh*> queued_blas_meshes_{}; // The queued meshes to be used to construct BLASes.
		std::vector<Blas*> blases_{};

		Context* context_{};
		Allocator* allocator_{};
	};
}
