#include "ray_tracing.h"

#include <algorithm>

#include "mesh.h"

namespace renderer
{
	void RayTracingContext::Initialize(Context* context, Allocator* allocator, DescriptorAllocator* descriptor_allocator, VulkanUtil* vulkan_util)
	{
		context_ = context;
		allocator_ = allocator;
		descriptor_allocator_ = descriptor_allocator;
		vulkan_util_ = vulkan_util;
		acceleration_structure_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
		rt_pipeline_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		acceleration_structure_properties_.pNext = &rt_pipeline_properties_;

		VkPhysicalDeviceProperties2 physical_device_properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &acceleration_structure_properties_,
		};

		// Get acceleration structure properties to reference throughout lifetime of RT context.
		vkGetPhysicalDeviceProperties2(context->physical_device, &physical_device_properties);

		CreatePipelineAndShaderBindingTable();
	}

	void RayTracingContext::CleanUp()
	{
		DeleteTemporaryBuffers();

		// Destroy shader binding table buffers.
		allocator_->DestroyBufferResource(&shader_binding_table_.raygen_sbt);
		allocator_->DestroyBufferResource(&shader_binding_table_.miss_sbt);
		allocator_->DestroyBufferResource(&shader_binding_table_.hit_sbt);

		vkDestroyPipeline(context_->device, rt_pipeline_, nullptr);
		vkDestroyPipelineLayout(context_->device, rt_pipeline_layout_, nullptr);
		vkDestroyDescriptorSetLayout(context_->device, rt_set_layout_resource_.layout, nullptr);
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
			std::string blas_name{ std::string{"Blas_"} + NameMesh(*build_info.geometries) };
			NameObject(context_->device, scratch_buffer.buffer, blas_name + "_Scratch_Buffer");

			// We must create the BLAS before we can build it.
			CreateAccelerationStructure(build_sizes.accelerationStructureSize, false, build_info.blas);
			NameObject(context_->device, build_info.blas->acceleration_structure, blas_name);
			NameObject(context_->device, build_info.blas->buffer_resource.buffer, blas_name + "_Buffer");

			blas_build_info.dstAccelerationStructure = build_info.blas->acceleration_structure;
			blas_build_info.scratchData.deviceAddress = DeviceAddress(context_->device, scratch_buffer.buffer);

			blas_build_infos.push_back(blas_build_info);
		}

		// Convert vector of vectors into vector of C pointers.
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> build_range_c_ptr_infos(build_range_infos.size());
		std::transform(build_range_infos.begin(), build_range_infos.end(), build_range_c_ptr_infos.begin(),
			[](std::vector<VkAccelerationStructureBuildRangeInfoKHR>& x) {
				return x.data();
			});

		vkCmdBuildAccelerationStructuresKHR(cmd, (uint32_t)blas_build_infos.size(), blas_build_infos.data(), build_range_c_ptr_infos.data());

		for (const QueuedBlasBuildInfo& build_info : queued_blas_build_infos_)
		{
			PipelineBarrier(cmd, build_info.blas->buffer_resource.buffer,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
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
						.data = DeviceAddress(context_->device, instance_buffer_.buffer),
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

			BufferResource scratch_buffer{ allocator_->CreateAlignedBufferResource(
				build_sizes.buildScratchSize, acceleration_structure_properties_.minAccelerationStructureScratchOffsetAlignment,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
			scratch_buffers_.push_back(scratch_buffer);
			NameObject(context_->device, scratch_buffer.buffer, "Tlas_Scratch_Buffer");

			// We must create the TLAS before we can build it.
			CreateAccelerationStructure(build_sizes.accelerationStructureSize, true, build_info.tlas);
			NameObject(context_->device, build_info.tlas->acceleration_structure, "Tlas");
			NameObject(context_->device, build_info.tlas->buffer_resource.buffer, "Tlas_Buffer");

			tlas_build_info.dstAccelerationStructure = build_info.tlas->acceleration_structure;
			tlas_build_info.scratchData.deviceAddress = DeviceAddress(context_->device, scratch_buffer.buffer);

			tlas_build_infos.push_back(tlas_build_info);
		}

		vkCmdBuildAccelerationStructuresKHR(cmd, (uint32_t)tlas_build_infos.size(), tlas_build_infos.data(), build_range_infos.data());

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

	void RayTracingContext::CmdTraceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth)
	{
		vkCmdTraceRaysKHR(cmd,
			&shader_binding_table_.raygen_sbt_address,
			&shader_binding_table_.miss_sbt_address,
			&shader_binding_table_.hit_sbt_address,
			&shader_binding_table_.callable_sbt_address,
			width,
			height,
			depth
		);
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
					.vertexData = DeviceAddress(context_->device, pmk_geometry.vertices_resource.buffer),
					.vertexStride = sizeof(Vertex),
					.maxVertex = *std::max_element(std::begin(pmk_geometry.indices), std::end(pmk_geometry.indices)),
					.indexType = VK_INDEX_TYPE_UINT16,
					.indexData = DeviceAddress(context_->device, pmk_geometry.indices_resource.buffer),
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

	BufferResource RayTracingContext::UploadInstancesToDevice(VkCommandBuffer cmd, const std::vector<VkAccelerationStructureInstanceKHR>& instances)
	{
		BufferResource device_buffer{ allocator_->CreateBufferResource(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
		NameObject(context_->device, device_buffer.buffer, "Ray_Tracing_Instance_Buffer");

		BufferResource staging{ allocator_->CreateBufferResource(device_buffer.size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };
		NameObject(context_->device, staging.buffer, "Ray_Tracing_Staging_Instance_Buffer");
		staging_buffers_.push_back(staging);

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

		PipelineBarrier(cmd, device_buffer.buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);

		return device_buffer;
	}

	void RayTracingContext::CreatePipelineAndShaderBindingTable()
	{
		ShaderBindingTableBuilder sbt_builder{};
		sbt_builder.Initialize(context_, allocator_, vulkan_util_, &rt_pipeline_properties_);
		sbt_builder.SetRaygenShader(SPIRV_PREFIX / "default.rgen.spv");
		sbt_builder.AddMissShader(SPIRV_PREFIX / "default.rmiss.spv");
		sbt_builder.AddHitGroup(SPIRV_PREFIX / "default.rchit.spv", SHADER_UNUSED_PATH, SHADER_UNUSED_PATH);

		CreatePipelineLayout();

		VkRayTracingPipelineCreateInfoKHR rt_pipeline_info{
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
			.flags = 0,
			.stageCount = (uint32_t)sbt_builder.GetShaderStages().size(),
			.pStages = sbt_builder.GetShaderStages().data(),
			.groupCount = (uint32_t)sbt_builder.GetGroups().size(),
			.pGroups = sbt_builder.GetGroups().data(),
			.maxPipelineRayRecursionDepth = 1,
			.pLibraryInfo = nullptr,
			.pLibraryInterface = nullptr,
			.pDynamicState = nullptr,
			.layout = rt_pipeline_layout_,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = {},
		};

		VkResult result{ vkCreateRayTracingPipelinesKHR(context_->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rt_pipeline_info, nullptr, &rt_pipeline_) };
		CheckResult(result, "Failed to create ray tracing pipeline.");
		NameObject(context_->device, rt_pipeline_, "Ray_Trace_Pipeline");

		shader_binding_table_ = sbt_builder.Build(rt_pipeline_);
		sbt_builder.CleanUp();
	}

	void RayTracingContext::CreatePipelineLayout()
	{
		VkDescriptorSetLayoutBinding tlas_binding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding image_buffer_binding{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding camera_ubo_binding{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> bindings{
			tlas_binding,
			image_buffer_binding,
			camera_ubo_binding,
		};

		rt_set_layout_resource_ = descriptor_allocator_->CreateDescriptorSetLayoutResource(bindings);
		NameObject(context_->device, rt_set_layout_resource_.layout, "Ray_Trace_Descriptor_Set_Layout");

		VkPipelineLayoutCreateInfo layout_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &rt_set_layout_resource_.layout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};

		VkResult result{ vkCreatePipelineLayout(context_->device, &layout_info, nullptr, &rt_pipeline_layout_) };
		CheckResult(result, "Failed to create ray tracing pipeline layout.");
		NameObject(context_->device, rt_pipeline_layout_, "Ray_Trace_Pipeline_Layout");
	}

	void ShaderBindingTableBuilder::Initialize(Context* context, Allocator* allocator, VulkanUtil* vulkan_util, VkPhysicalDeviceRayTracingPipelinePropertiesKHR* rt_pipeline_properties)
	{
		context_ = context;
		allocator_ = allocator;
		vulkan_util_ = vulkan_util;
		rt_pipeline_properties_ = rt_pipeline_properties;
	}

	void ShaderBindingTableBuilder::CleanUp()
	{
		for (const VkPipelineShaderStageCreateInfo& stage_info : shader_stages_) {
			vkDestroyShaderModule(context_->device, stage_info.module, nullptr);
		}
	}

	void ShaderBindingTableBuilder::SetRaygenShader(const std::filesystem::path& spirv_path)
	{
		VkShaderModule shader_module{ LoadShaderModule(context_->device, spirv_path) };

		VkPipelineShaderStageCreateInfo shader_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0, // Flags are all about subgroup sizes.
			.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			.module = shader_module,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		uint32_t shader_stage_index{ (uint32_t)shader_stages_.size() };
		shader_stages_.push_back(shader_stage);

		raygen_group_ = VkRayTracingShaderGroupCreateInfoKHR{
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = shader_stage_index,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR,
			.pShaderGroupCaptureReplayHandle = nullptr,
		};

		concatenated_groups_dirty_ = true;
	}

	void ShaderBindingTableBuilder::AddMissShader(const std::filesystem::path& spirv_path)
	{
		VkShaderModule shader_module{ LoadShaderModule(context_->device, spirv_path) };

		VkPipelineShaderStageCreateInfo shader_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0, // Flags are all about subgroup sizes.
			.stage = VK_SHADER_STAGE_MISS_BIT_KHR,
			.module = shader_module,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		uint32_t shader_stage_index{ (uint32_t)shader_stages_.size() };
		shader_stages_.push_back(shader_stage);

		VkRayTracingShaderGroupCreateInfoKHR shader_group{
		   .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		   .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		   .generalShader = shader_stage_index,
		   .closestHitShader = VK_SHADER_UNUSED_KHR,
		   .anyHitShader = VK_SHADER_UNUSED_KHR,
		   .intersectionShader = VK_SHADER_UNUSED_KHR,
		   .pShaderGroupCaptureReplayHandle = nullptr,
		};

		miss_groups_.push_back(std::move(shader_group));
		concatenated_groups_dirty_ = true;
	}

	void ShaderBindingTableBuilder::AddHitGroup(const std::filesystem::path& closest_hit, const std::filesystem::path& any_hit, const std::filesystem::path& intersection)
	{
		uint32_t closest_hit_stage_index{ VK_SHADER_UNUSED_KHR };
		uint32_t any_hit_stage_index{ VK_SHADER_UNUSED_KHR };
		uint32_t intersection_stage_index{ VK_SHADER_UNUSED_KHR };

		if (closest_hit != SHADER_UNUSED_PATH) {
			closest_hit_stage_index = AddHitShader(closest_hit, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		}

		if (any_hit != SHADER_UNUSED_PATH) {
			any_hit_stage_index = AddHitShader(any_hit, VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		}

		if (intersection != SHADER_UNUSED_PATH) {
			intersection_stage_index = AddHitShader(intersection, VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
		}

		VkRayTracingShaderGroupTypeKHR group_type{ intersection_stage_index == VK_SHADER_UNUSED_KHR ?
			VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR : VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR };

		VkRayTracingShaderGroupCreateInfoKHR shader_group{
		   .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		   .type = group_type,
		   .generalShader = VK_SHADER_UNUSED_KHR,
		   .closestHitShader = closest_hit_stage_index,
		   .anyHitShader = any_hit_stage_index,
		   .intersectionShader = intersection_stage_index,
		   .pShaderGroupCaptureReplayHandle = nullptr,
		};

		hit_groups_.push_back(std::move(shader_group));
		concatenated_groups_dirty_ = true;
	}

	const std::vector<VkPipelineShaderStageCreateInfo>& ShaderBindingTableBuilder::GetShaderStages() const
	{
		return shader_stages_;
	}

	const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& ShaderBindingTableBuilder::GetGroups()
	{
		if (!concatenated_groups_dirty_) {
			return concatenated_groups_;
		}

		concatenated_groups_.clear();
		concatenated_groups_.reserve(1 + miss_groups_.size() + hit_groups_.size());
		concatenated_groups_.push_back(raygen_group_);
		concatenated_groups_.insert(concatenated_groups_.end(), miss_groups_.begin(), miss_groups_.end());
		concatenated_groups_.insert(concatenated_groups_.end(), hit_groups_.begin(), hit_groups_.end());

		concatenated_groups_dirty_ = false;
		return concatenated_groups_;
	}

	ShaderBindingTable ShaderBindingTableBuilder::Build(VkPipeline pipeline)
	{
		uint32_t aligned_handle_size{ AlignUp(rt_pipeline_properties_->shaderGroupHandleSize, rt_pipeline_properties_->shaderGroupHandleAlignment) };

		// Make buffers.
		ShaderBindingTable sbt{};
		sbt.raygen_sbt = allocator_->CreateAlignedBufferResource(aligned_handle_size, rt_pipeline_properties_->shaderGroupBaseAlignment,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, sbt.raygen_sbt.buffer, "Raygen_SBT_Buffer");
		sbt.raygen_sbt_address.deviceAddress = DeviceAddress(context_->device, sbt.raygen_sbt.buffer);
		sbt.raygen_sbt_address.stride = aligned_handle_size;
		sbt.raygen_sbt_address.size = sbt.raygen_sbt.size;

		sbt.miss_sbt = allocator_->CreateAlignedBufferResource(aligned_handle_size * (uint32_t)miss_groups_.size(), rt_pipeline_properties_->shaderGroupBaseAlignment,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, sbt.miss_sbt.buffer, "Miss_SBT_Buffer");
		sbt.miss_sbt_address.deviceAddress = DeviceAddress(context_->device, sbt.miss_sbt.buffer);
		sbt.miss_sbt_address.stride = aligned_handle_size;
		sbt.miss_sbt_address.size = sbt.miss_sbt.size;

		sbt.hit_sbt = allocator_->CreateAlignedBufferResource(aligned_handle_size * (uint32_t)hit_groups_.size(), rt_pipeline_properties_->shaderGroupBaseAlignment,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, sbt.hit_sbt.buffer, "Hit_SBT_Buffer");
		sbt.hit_sbt_address.deviceAddress = DeviceAddress(context_->device, sbt.hit_sbt.buffer);
		sbt.hit_sbt_address.stride = aligned_handle_size;
		sbt.hit_sbt_address.size = sbt.hit_sbt.size;

		// Query handles from pipeline.
		std::vector<uint8_t> group_handles(aligned_handle_size * (uint32_t)GetGroups().size());
		VkResult result{ vkGetRayTracingShaderGroupHandlesKHR(context_->device, pipeline, 0, (uint32_t)GetGroups().size(), (uint32_t)group_handles.size(), group_handles.data()) };
		CheckResult(result, "Failed getting shader group handles.");

		// Transfer handle data to buffers.
		uint8_t* handle_ptr{ group_handles.data() };
		vulkan_util_->Begin();

		vulkan_util_->TransferBufferToDevice(handle_ptr, aligned_handle_size, sbt.raygen_sbt);
		handle_ptr += aligned_handle_size;

		uint32_t miss_group_bytes{ aligned_handle_size * (uint32_t)miss_groups_.size() };
		vulkan_util_->TransferBufferToDevice(handle_ptr, miss_group_bytes, sbt.miss_sbt);
		handle_ptr += miss_group_bytes;

		uint32_t hit_group_bytes{ aligned_handle_size * (uint32_t)hit_groups_.size() };
		vulkan_util_->TransferBufferToDevice(handle_ptr, hit_group_bytes, sbt.hit_sbt);
		vulkan_util_->Submit();

		return sbt;
	}

	uint32_t ShaderBindingTableBuilder::AddHitShader(const std::filesystem::path& shader, VkShaderStageFlagBits shader_stage_flag)
	{
		VkShaderModule shader_module{ LoadShaderModule(context_->device, shader) };

		VkPipelineShaderStageCreateInfo shader_stage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.flags = 0, // Flags are all about subgroup sizes.
			.stage = shader_stage_flag,
			.module = shader_module,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		if (hit_shader_index_map_.find(shader) != hit_shader_index_map_.end()) {
			return hit_shader_index_map_[shader];
		}

		uint32_t shader_index = (uint32_t)shader_stages_.size();
		shader_stages_.push_back(shader_stage);
		hit_shader_index_map_[shader] = shader_index;
		return shader_index;
	}
}