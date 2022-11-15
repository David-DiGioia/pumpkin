#include "ray_tracing.h"

#include "vulkan_util.h"

#include <algorithm>
#include "mesh.h"

namespace renderer
{
	void RayTracingContext::Initialize(Context* context, Allocator* alloc)
	{
		context_ = context;
		allocator_ = alloc;
		acceleration_structure_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

		VkPhysicalDeviceProperties2 physical_device_properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &acceleration_structure_properties_,
		};

		// Get acceleration structure properties to reference throughout lifetime of RT context.
		vkGetPhysicalDeviceProperties2(context->physical_device, &physical_device_properties);
	}

	void RayTracingContext::CleanUp()
	{
		DeleteTemporaryBuffers();
	}

	void RayTracingContext::QueueBlas(Mesh* mesh)
	{
		QueuedBlasBuildInfo build_info{
			.blas = &mesh->blas, // This BLAS will be populated later when build command is called.
			.geometries = &mesh->geometries,
		};

		queued_blas_build_infos_.push_back(build_info);
	}

	AccelerationStructure* RayTracingContext::QueueTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances)
	{
		QueuedTlasBuildInfo build_info{
			.tlas = new AccelerationStructure{}, // This TLAS will be populated later when build command is called.
			.instances = &instances,
		};

		queued_tlas_build_infos_.push_back(build_info);

		return build_info.tlas;
	}

	void RayTracingContext::CmdBuildQueuedBlases(VkCommandBuffer cmd)
	{
		logger::Print("Recording commands to build BLASes.\n");

		// Vector of arrays of geometry build range infos.
		std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> build_range_infos{};
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blas_build_infos{};
		std::vector<std::vector<VkAccelerationStructureGeometryKHR>> all_vk_geometries{}; // Need to store in function scope so it isn't deallocated before build command.
		all_vk_geometries.reserve(queued_blas_build_infos_.size()); // Needed so pointers don't become invalidated before calling build command.

		for (const QueuedBlasBuildInfo& build_info : queued_blas_build_infos_)
		{
			std::vector<VkAccelerationStructureBuildRangeInfoKHR>& geometry_range_infos{ build_range_infos.emplace_back() };
			std::vector<VkAccelerationStructureGeometryKHR>& vk_geometries{ all_vk_geometries.emplace_back() };

			for (const Geometry& pmk_geometry : *build_info.geometries)
			{
				vk_geometries.push_back(PumpkinTriGeometryToVulkanGeometry(pmk_geometry));

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

			VkAccelerationStructureBuildSizesInfoKHR build_sizes{ GetAccelerationStructureBuildSizes(blas_build_info, *build_info.geometries) };

			BufferResource scratch_buffer{ allocator_->CreateAlignedBufferResource(
				build_sizes.buildScratchSize, acceleration_structure_properties_.minAccelerationStructureScratchOffsetAlignment,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
			scratch_buffers_.push_back(scratch_buffer);

			// We must create the BLAS before we can build it.
			CreateAccelerationStructure(build_sizes.accelerationStructureSize, false, build_info.blas);

			blas_build_info.dstAccelerationStructure = build_info.blas->acceleration_structure;
			blas_build_info.scratchData.deviceAddress = DeviceAddress(scratch_buffer.buffer);

			blas_build_infos.push_back(blas_build_info);
		}

		// Convert vector of vectors into vector of C pointers.
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> build_range_c_ptr_infos(build_range_infos.size());
		std::transform(build_range_infos.begin(), build_range_infos.end(), build_range_c_ptr_infos.begin(),
			[](std::vector<VkAccelerationStructureBuildRangeInfoKHR>& x) {
				return x.data();
			});

		vkCmdBuildAccelerationStructuresKHR(cmd, blas_build_infos.size(), blas_build_infos.data(), build_range_c_ptr_infos.data());

		for (const QueuedBlasBuildInfo& build_info : queued_blas_build_infos_)
		{
			PipelineBarrier(cmd, build_info.blas->buffer_resource.buffer,
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		} 
		queued_blas_build_infos_.clear();
	}

	void RayTracingContext::CmdBuildQueuedTlases(VkCommandBuffer cmd)
	{
		// Since each TLAS only has 1 geometry, we don't need a vector of vectors for build range infos.
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> build_range_infos{};
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> tlas_build_infos{};

		for (const QueuedTlasBuildInfo& build_info : queued_tlas_build_infos_)
		{
			instance_buffer_ = UploadInstancesToDevice(cmd, *build_info.instances);

			VkAccelerationStructureGeometryKHR vk_geometry{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
				.geometry = {
					.instances = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
						.arrayOfPointers = VK_FALSE,
						.data = DeviceAddress(instance_buffer_.buffer),
					},
				},
				.flags = 0,
			};

			VkAccelerationStructureBuildRangeInfoKHR* range_info{ new VkAccelerationStructureBuildRangeInfoKHR{
				.primitiveCount = (uint32_t)build_info.instances->size(),
				.primitiveOffset = 0,
				.firstVertex = 0,
				.transformOffset = 0,
			} };

			build_range_infos.push_back(range_info);

			VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
				.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
				.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
				.srcAccelerationStructure = VK_NULL_HANDLE,
				.dstAccelerationStructure = VK_NULL_HANDLE, // Populate this after getting acceleration structure size.
				.geometryCount = 1,
				.pGeometries = &vk_geometry,
				.scratchData = {}, // Populate this after getting scratch buffer size.
			};

			VkAccelerationStructureBuildSizesInfoKHR build_sizes{ GetAccelerationStructureBuildSizes(tlas_build_info, (uint32_t)build_info.instances->size()) };

			BufferResource scratch_buffer{ allocator_->CreateBufferResource(build_sizes.buildScratchSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
			scratch_buffers_.push_back(scratch_buffer);

			// We must create the BLAS before we can build it.
			CreateAccelerationStructure(build_sizes.accelerationStructureSize, true, build_info.tlas);

			tlas_build_info.dstAccelerationStructure = build_info.tlas->acceleration_structure;
			tlas_build_info.scratchData.deviceAddress = DeviceAddress(scratch_buffer.buffer);

			tlas_build_infos.push_back(tlas_build_info);
		}

		vkCmdBuildAccelerationStructuresKHR(cmd, tlas_build_infos.size(), tlas_build_infos.data(), build_range_infos.data());
	
		for (const QueuedTlasBuildInfo& build_info : queued_tlas_build_infos_)
		{
			PipelineBarrier(cmd, build_info.tlas->buffer_resource.buffer,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
		}
		queued_tlas_build_infos_.clear();

		for (VkAccelerationStructureBuildRangeInfoKHR* range_info : build_range_infos) {
			delete range_info;
		}
	}

	void RayTracingContext::DeleteTemporaryBuffers()
	{
		for (BufferResource buffer_resource : scratch_buffers_) {
			allocator_->DestroyBufferResource(&buffer_resource);
		}
		scratch_buffers_.clear();

		for (BufferResource buffer_resource : staging_buffers_) {
			allocator_->DestroyBufferResource(&buffer_resource);
		}
		staging_buffers_.clear();

		allocator_->DestroyBufferResource(&instance_buffer_);
	}

	VkAccelerationStructureGeometryKHR RayTracingContext::PumpkinTriGeometryToVulkanGeometry(const Geometry& pmk_geometry) const
	{
		VkAccelerationStructureGeometryKHR vk_geometry{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
			.geometry = {
				.triangles = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
					.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
					.vertexData = DeviceAddress(pmk_geometry.vertices_resource.buffer),
					.vertexStride = sizeof(Vertex),
					.maxVertex = *std::max_element(std::begin(pmk_geometry.indices), std::end(pmk_geometry.indices)),
					.indexType = VK_INDEX_TYPE_UINT32,
					.indexData = DeviceAddress(pmk_geometry.indices_resource.buffer),
					.transformData = {},
				},
			},
			.flags = 0,
		};

		return vk_geometry;
	}

	void RayTracingContext::CreateAccelerationStructure(VkDeviceSize acceleration_structure_size, bool top_level, AccelerationStructure* out_acceleration_structure) const
	{
		out_acceleration_structure->buffer_resource = allocator_->CreateBufferResource(acceleration_structure_size,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkAccelerationStructureCreateInfoKHR as_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.createFlags = 0,
			.buffer = out_acceleration_structure->buffer_resource.buffer,
			.offset = 0,
			.size = acceleration_structure_size,
			.type = top_level ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			.deviceAddress = {}, // Unused. For capture replay feature.
		};

		VkResult result{ vkCreateAccelerationStructureKHR(context_->device, &as_info, nullptr, &out_acceleration_structure->acceleration_structure) };
		CheckResult(result, "Failed to create acceleration structure.");
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
		std::vector<uint32_t> max_primitive_counts(geometries.size());
		std::transform(geometries.begin(), geometries.end(), max_primitive_counts.begin(),
			[](const Geometry& x) {
				return x.indices.size() / 3;
			});

		vkGetAccelerationStructureBuildSizesKHR(context_->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, max_primitive_counts.data(), &build_sizes_info);
		return build_sizes_info;
	}

	VkAccelerationStructureBuildSizesInfoKHR RayTracingContext::GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& build_info, uint32_t instance_count) const
	{
		VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
			.accelerationStructureSize = 0, // Out variable.
			.updateScratchSize = 0, // Out variable.
			.buildScratchSize = 0, // Out variable.
		};

		vkGetAccelerationStructureBuildSizesKHR(context_->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &instance_count, &build_sizes_info);
		return build_sizes_info;
	}

	VkDeviceAddress RayTracingContext::DeviceAddress(VkBuffer buffer) const
	{
		VkBufferDeviceAddressInfo device_address_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = buffer,
		};

		auto address = vkGetBufferDeviceAddress(context_->device, &device_address_info);
		logger::Print("Device address: 0x%x\n", address);
		return address;
	}

	BufferResource RayTracingContext::UploadInstancesToDevice(VkCommandBuffer cmd, const std::vector<VkAccelerationStructureInstanceKHR>& instances)
	{
		BufferResource device_buffer{ allocator_->CreateBufferResource(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };

		BufferResource staging{ allocator_->CreateBufferResource(device_buffer.size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };

		// Copy data to staging buffer.
		void* data{};
		vkMapMemory(context_->device, *staging.memory, staging.offset, staging.size, 0, &data);
		memcpy(data, instances.data(), staging.size);
		vkUnmapMemory(context_->device, *staging.memory);

		// Transfer from staging to device.
		VkBufferCopy buffer_copy{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = staging.size,
		};

		vkCmdCopyBuffer(cmd, staging.buffer, device_buffer.buffer, 1, &buffer_copy);

		// Store it to destroy after acceleration structure is built.
		staging_buffers_.push_back(staging);

		PipelineBarrier(cmd, device_buffer.buffer,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

		return device_buffer;
	}
}