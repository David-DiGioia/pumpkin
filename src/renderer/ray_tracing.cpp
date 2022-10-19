#include "ray_tracing.h"

#include "vulkan_util.h"

#include <algorithm>

namespace renderer
{
	void RayTracingContext::Initialize(Context* context, Allocator* alloc)
	{
		context_ = context;
		allocator_ = alloc;
	}

	void RayTracingContext::CleanUp()
	{
		for (Blas* blas : blases_)
		{
			allocator_->DestroyBufferResource(&blas->buffer_resource);
			delete blas;
		}
	}

	void RayTracingContext::AddBlas(const Mesh& mesh)
	{
		queued_blas_meshes_.push_back(&mesh);
	}

	VkAccelerationStructureGeometryKHR RayTracingContext::PumpkinGeometryToVulkanGeometry(const Geometry& pmk_geometry) const
	{
		VkBufferDeviceAddressInfo vertex_address_info{
					.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
					.buffer = pmk_geometry.vertices_resource.buffer,
		};

		VkBufferDeviceAddressInfo index_address_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = pmk_geometry.indices_resource.buffer,
		};

		VkAccelerationStructureGeometryKHR vk_geometry{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
			.geometry = {
				.triangles = {
					.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
					.vertexData = vkGetBufferDeviceAddress(context_->device, &vertex_address_info),
					.vertexStride = sizeof(Vertex),
					.maxVertex = *std::max_element(std::begin(pmk_geometry.indices), std::end(pmk_geometry.indices)),
					.indexType = VK_INDEX_TYPE_UINT32,
					.indexData = vkGetBufferDeviceAddress(context_->device, &index_address_info),
					.transformData = {},
				},
			},
			.flags = 0,
		};

		return vk_geometry;
	}

	void RayTracingContext::CreateBlas(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const VkAccelerationStructureBuildSizesInfoKHR& build_sizes, Blas* out_blas) const
	{
		out_blas->buffer_resource = allocator_->CreateBufferResource(build_sizes.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkAccelerationStructureCreateInfoKHR blas_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.createFlags = 0,
			.buffer = out_blas->buffer_resource.buffer,
			.offset = 0,
			.size = build_sizes.accelerationStructureSize,
			.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			.deviceAddress = {}, // Unused. For capture replay feature.
		};

		VkResult result{ vkCreateAccelerationStructureKHR(context_->device, &blas_info, nullptr, &out_blas->blas) };
		CheckResult(result, "Failed to create BLAS.");
	}

	VkAccelerationStructureBuildSizesInfoKHR RayTracingContext::GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, const std::vector<Geometry>& geometries) const
	{
		VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
			.accelerationStructureSize = 0, // Out variable.
			.updateScratchSize = 0, // Out variable.
			.buildScratchSize = 0, // Out variable.
		};

		// Get number of triangles in each geometry.
		std::vector<uint32_t> max_primitive_counts{};
		std::transform(geometries.begin(), geometries.end(), max_primitive_counts.begin(),
			[](const Geometry& x) {
				return x.indices.size() / 3;
			});

		vkGetAccelerationStructureBuildSizesKHR(context_->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, max_primitive_counts.data(), &build_sizes_info);
		return build_sizes_info;
	}

	VkDeviceAddress RayTracingContext::DeviceAddress(VkBuffer buffer) const
	{
		VkBufferDeviceAddressInfo device_address_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = buffer,
		};

		return vkGetBufferDeviceAddress(context_->device, &device_address_info);
	}

	void RayTracingContext::BuildBlases(VkCommandBuffer cmd)
	{
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blas_build_infos{};
		// Vector of arrays of geometry build range infos.
		std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> build_range_infos{};

		for (const Mesh* mesh : queued_blas_meshes_)
		{
			std::vector<VkAccelerationStructureBuildRangeInfoKHR> geometry_range_infos = build_range_infos.emplace_back();
			std::vector<VkAccelerationStructureGeometryKHR> vk_geometries{};

			for (const Geometry& pmk_geometry : mesh->geometries)
			{
				vk_geometries.push_back(PumpkinGeometryToVulkanGeometry(pmk_geometry));

				VkAccelerationStructureBuildRangeInfoKHR range_info{
					.primitiveCount = (uint32_t)(pmk_geometry.indices.size() / 3),
					.primitiveOffset = 0,
					.firstVertex = 0,
					.transformOffset = 0,
				};

				geometry_range_infos.push_back(range_info);
			}

			VkAccelerationStructureBuildGeometryInfoKHR blas_build_info{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
				.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
				.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
				.srcAccelerationStructure = VK_NULL_HANDLE,
				.dstAccelerationStructure = VK_NULL_HANDLE, // Populate this after getting acceleration structure size.
				.geometryCount = (uint32_t)vk_geometries.size(),
				.pGeometries = vk_geometries.data(),
				.scratchData = {}, // Populate this after getting scratch buffer size.
			};

			VkAccelerationStructureBuildSizesInfoKHR build_sizes{ GetAccelerationStructureBuildSizes(blas_build_info, mesh->geometries) };

			BufferResource scratch_buffer{ allocator_->CreateBufferResource(build_sizes.buildScratchSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

			// We must create the BLAS before we can build it.
			Blas* blas{ new Blas{} };
			CreateBlas(blas_build_info, build_sizes, blas);

			blas_build_info.dstAccelerationStructure = blas->blas;
			blas_build_info.scratchData.deviceAddress = DeviceAddress(scratch_buffer.buffer);

			blases_.push_back(blas);
			blas_build_infos.push_back(blas_build_info);
		}

		// Convert vector of vectors into vector of C pointers.
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> build_range_c_ptr_infos{};
		std::transform(build_range_infos.begin(), build_range_infos.end(), build_range_c_ptr_infos.begin(),
			[](std::vector<VkAccelerationStructureBuildRangeInfoKHR>& x) {
				return x.data();
			});

		vkCmdBuildAccelerationStructuresKHR(cmd, blas_build_infos.size(), blas_build_infos.data(), build_range_c_ptr_infos.data());
	}
}