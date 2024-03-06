#include "particle_gen.h"

#include <execution>
#include "tracy/Tracy.hpp"

#include "vulkan_renderer.h"
#include "vulkan_util.h"
#include "common_constants.h"

namespace renderer
{
	// Particle gen shader descriptor set.
	constexpr uint32_t PARTICLE_GEN_DESCRIPTOR_SET{ 0 };
	constexpr uint32_t PARTICLE_BUILT_IN_UBO_BINDING{ 0 };
	constexpr uint32_t PARTICLE_CUSTOM_UBO_BINDING{ 1 };
	constexpr uint32_t PARTICLE_OUT_PARTICLE_SSBO_BINDING{ 2 };
	// Particle neighbor shader descriptor set.
	constexpr uint32_t PARTICLE_NEIGHBOR_DESCRIPTOR_SET{ 0 };
	constexpr uint32_t PARTICLE_NEIGHBOR_IN_PARTICLE_SSBO_BINDING{ 0 };
	constexpr uint32_t PARTICLE_NEIGHBOR_OUT_NEIGHBOR_SSBO_BINDING{ 1 };
	// Particle mesh generation shader descriptor set.
	constexpr uint32_t PARTICLE_MESH_DESCRIPTOR_SET{ 0 };
	constexpr uint32_t PARTICLE_MESH_IN_POSITIONS_BINDING{ 0 };
	constexpr uint32_t PARTICLE_MESH_OUT_VERTICES_BINDING{ 1 };
	constexpr uint32_t PARTICLE_MESH_OUT_INDICES_BINDING{ 2 };
	constexpr uint32_t PARTICLE_MESH_UBO_BINDING{ 3 };

	std::vector<MaterialRange> ParticleGenContext::GetMaterialRanges()
	{
		return mat_ranges_;
	}

	VoxelChunk::VoxelChunk(uint32_t width, uint32_t height, uint32_t depth)
		: width_{ width }
		, height_{ height }
		, depth_{ depth }
		, width_height_slice_{ width * height }
	{
		Voxel empty_voxel{
			.physics_material_index = PHYSICS_MATERIAL_EMPTY_INDEX,
		};

		const uint64_t voxel_count{ (uint64_t)width_height_slice_ * depth_ };
		voxels_.resize(voxel_count, empty_voxel);
		side_flags_.resize(voxel_count);
	}

	VoxelChunk::VoxelChunk(uint32_t width, uint32_t height, uint32_t depth, std::vector<std::pair<Voxel, glm::uvec3>>&& voxel_pairs)
		: VoxelChunk(width, height, depth)
	{
		// Convert the voxel pairs into voxels in the voxels_ vector.
		std::for_each(
			std::execution::par,
			voxel_pairs.begin(),
			voxel_pairs.end(),
			[&](std::pair<Voxel, glm::uvec3>& pair)
			{
				Coordinate(pair.second.x, pair.second.y, pair.second.z) = pair.first;
			});

		// Create side flags.
		std::for_each(
			std::execution::par,
			voxel_pairs.begin(),
			voxel_pairs.end(),
			[&](std::pair<Voxel, glm::uvec3>& pair)
			{
				glm::uvec3& coord{ pair.second };
				uint8_t neighbors{};

				// X-axis neighbors.
				if (coord.x != width_ - 1 && NeighborOccupied(coord, glm::ivec3(1, 0, 0))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::X_POSITIVE;
				}
				if (coord.x != 0 && NeighborOccupied(coord, glm::ivec3(-1, 0, 0))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::X_NEGATIVE;
				}
				// Y-axis neighbors.
				if (coord.y != height_ - 1 && NeighborOccupied(coord, glm::ivec3(0, 1, 0))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::Y_POSITIVE;
				}
				if (coord.y != 0 && NeighborOccupied(coord, glm::ivec3(0, -1, 0))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::Y_NEGATIVE;
				}
				// Y-axis neighbors.
				if (coord.z != depth_ - 1 && NeighborOccupied(coord, glm::ivec3(0, 0, 1))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::Z_POSITIVE;
				}
				if (coord.z != 0 && NeighborOccupied(coord, glm::ivec3(0, 0, -1))) {
					neighbors |= (uint8_t)ParticleSidesFlagBits::Z_NEGATIVE;
				}

				side_flags_[CoordinateToIndex(coord)] = neighbors;
			});
	}

	Voxel& VoxelChunk::Coordinate(uint32_t i, uint32_t j, uint32_t k)
	{
		return voxels_[CoordinateToIndex({ i, j, k })];
	}

	Voxel& VoxelChunk::Coordinate(const glm::uvec3& coord)
	{
		return Coordinate(coord.x, coord.y, coord.z);
	}

	const Voxel& VoxelChunk::Coordinate(uint32_t i, uint32_t j, uint32_t k) const
	{
		return voxels_[CoordinateToIndex({ i, j, k })];
	}

	const Voxel& VoxelChunk::Coordinate(const glm::uvec3& coord) const
	{
		return Coordinate(coord.x, coord.y, coord.z);
	}

	Voxel& VoxelChunk::Index(uint32_t idx)
	{
		return voxels_[idx];
	}

	const Voxel& VoxelChunk::Index(uint32_t idx) const
	{
		return voxels_[idx];
	}

	uint32_t VoxelChunk::VoxelCount() const
	{
		return (uint32_t)voxels_.size();
	}

	bool VoxelChunk::IsOccluded(uint32_t voxel_idx) const
	{
		return side_flags_[voxel_idx] == (uint8_t)ParticleSidesFlagBits::ALL_SIDES;
	}

	bool VoxelChunk::IsEmpty(uint32_t voxel_idx) const
	{
		return voxels_[voxel_idx].physics_material_index == PHYSICS_MATERIAL_EMPTY_INDEX;
	}

	glm::uvec3 VoxelChunk::IndexToCoordinate(uint32_t index) const
	{
		uint32_t z{ index / width_height_slice_ };
		uint32_t y{ (index % width_height_slice_) / width_ };
		uint32_t x{ index % width_ };

		return glm::uvec3{ x, y, z };
	}

	uint32_t VoxelChunk::CoordinateToIndex(const glm::uvec3& coord) const
	{
		return coord.x + coord.y * width_ + coord.z * width_height_slice_;
	}

	std::vector<Voxel>& VoxelChunk::GetVoxels()
	{
		return voxels_;
	}

	std::vector<uint8_t>& VoxelChunk::GetSideFlags()
	{
		return side_flags_;
	}

	bool VoxelChunk::NeighborOccupied(glm::uvec3 coord, glm::ivec3 offset)
	{
		glm::uvec3 neighbor_coord = glm::ivec3{ coord } + offset;
		return IsEmpty(CoordinateToIndex(neighbor_coord));
	}

	void ParticleGenContext::Initialize(Context* context, VulkanRenderer* renderer)
	{
		context_ = context;
		renderer_ = renderer;

		InitializeParticleGenShaderResources();
		InitializeParticleNeighborsShaderResources();
		InitializeParticleMeshShaderResources();
		InitializeCommandBuffers();
		InitializeFences();
	}

	void ParticleGenContext::CleanUp()
	{
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.built_in_ubo_buffer);
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.particle_out_buffer);
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.custom_ubo_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_gen_.layout_resource);

		renderer_->allocator_.DestroyBufferResource(&particle_neighbors_.neighbor_out_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_neighbors_.layout_resource);
		particle_neighbors_.pipeline.CleanUp();

		for (FrameResources& frame : frame_resources_)
		{
			renderer_->allocator_.DestroyBufferResource(&frame.particle_mesh.ubo_buffer);
			renderer_->allocator_.DestroyBufferResource(&frame.particle_mesh.positions_in);
			renderer_->allocator_.DestroyBufferResource(&frame.particle_mesh.vertices_out);
			renderer_->allocator_.DestroyBufferResource(&frame.particle_mesh.indices_out);
			vkDestroyFence(context_->device, frame.fence, nullptr);
		}
		vkDestroyDescriptorSetLayout(context_->device, particle_mesh_.layout_resource.layout, nullptr);
		particle_mesh_.pipeline.CleanUp();
	}

	void ParticleGenContext::InvokeParticleGenShader(RenderObjectHandle ro_target, std::vector<Voxel>* out_voxels, std::vector<uint8_t>* out_side_flags)
	{
		ComputePipeline* particle_gen_pipeline{ renderer_->user_compute_shaders_[particle_gen_.shader_idx] };

		// Update built in UBO before invoking shader.
		ParticleGenShaderResources::BuiltInUBO built_in_ubo{
			.chunk_coordinate = {0, 0, 0},
		};
		void* data{};
		vkMapMemory(context_->device, *particle_gen_.built_in_ubo_buffer.memory, particle_gen_.built_in_ubo_buffer.offset, particle_gen_.built_in_ubo_buffer.size, 0, &data);
		memcpy(data, &built_in_ubo, sizeof(ParticleGenShaderResources::BuiltInUBO));
		vkUnmapMemory(context_->device, *particle_gen_.built_in_ubo_buffer.memory);

		// Record and submit command buffer.
		VkCommandBuffer cmd{ renderer_->vulkan_util_.Begin() };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, particle_gen_pipeline->layout, PARTICLE_GEN_DESCRIPTOR_SET, 1, &particle_gen_.descriptor_set_resource.descriptor_set, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, particle_gen_pipeline->pipeline);

		// Calculate voxels.
		// Work group count of 16 on each dimension with local_group size of 4 on each access for 64x64x64 dispatch size.
		vkCmdDispatch(cmd, PARTICLE_GROUP_COUNT, PARTICLE_GROUP_COUNT, PARTICLE_GROUP_COUNT);

		renderer_->vulkan_util_.PipelineBarrier(
			particle_gen_.particle_out_buffer.buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		// Calculate neighbors of static particles.
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, particle_neighbors_.pipeline.layout, PARTICLE_NEIGHBOR_DESCRIPTOR_SET, 1, &particle_neighbors_.descriptor_set_resource.descriptor_set, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, particle_neighbors_.pipeline.pipeline);
		vkCmdDispatch(cmd, PARTICLE_GROUP_COUNT, PARTICLE_GROUP_COUNT, PARTICLE_GROUP_COUNT);

		renderer_->vulkan_util_.PipelineBarrier(
			particle_gen_.particle_out_buffer.buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		out_voxels->clear();
		out_voxels->resize(CHUNK_TOTAL_VOXEL_COUNT);
		renderer_->vulkan_util_.TransferBufferToHost(*out_voxels, particle_gen_.particle_out_buffer);
		renderer_->vulkan_util_.Submit();

		// Copy the particle neighbor data to a vector.
		// We don't use TransferBufferToHost because side flags are host visible so we avoid using a staging buffer this way.
		out_side_flags->clear();
		out_side_flags->resize(CHUNK_TOTAL_VOXEL_COUNT);
		vkMapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory, particle_neighbors_.neighbor_out_buffer.offset, particle_neighbors_.neighbor_out_buffer.size, 0, &data);
		std::memcpy(out_side_flags->data(), data, sizeof(uint8_t) * CHUNK_TOTAL_VOXEL_COUNT);
		vkUnmapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory);
	}

	void ParticleGenContext::SetParticleGenShader(uint32_t shader_idx, uint32_t custom_ubo_size)
	{
		particle_gen_.shader_idx = shader_idx;

		if (particle_gen_.custom_ubo_buffer.buffer != VK_NULL_HANDLE)
		{
			renderer_->allocator_.DestroyBufferResource(&particle_gen_.custom_ubo_buffer);
			particle_gen_.custom_ubo_buffer.buffer = VK_NULL_HANDLE;
		}

		particle_gen_.custom_ubo_buffer = renderer_->allocator_.CreateBufferResource(
			(VkDeviceSize)custom_ubo_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, particle_gen_.custom_ubo_buffer.buffer, "Particle_Gen_Custom_Ubo_Buffer");

		particle_gen_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_CUSTOM_UBO_BINDING, particle_gen_.custom_ubo_buffer);
	}

	void ParticleGenContext::UpdateParticleGenShaderCustomUBO(const std::vector<std::byte>& custom_ubo)
	{
		renderer_->vulkan_util_.Begin();
		renderer_->vulkan_util_.TransferBufferToDevice(custom_ubo, particle_gen_.custom_ubo_buffer);
		renderer_->vulkan_util_.Submit();
	}

	DescriptorSetLayoutResource& ParticleGenContext::GetParticleGenLayoutResource()
	{
		return particle_gen_.layout_resource;
	}

	void ParticleGenContext::InitializeParticleGenShaderResources()
	{
		// Make descriptor set layout.
		VkDescriptorSetLayoutBinding built_in_ubo_binding{
			.binding = PARTICLE_BUILT_IN_UBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding custom_ubo_binding{
			.binding = PARTICLE_CUSTOM_UBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding out_particle_ssbo{
			.binding = PARTICLE_OUT_PARTICLE_SSBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> layout_bindings{
			built_in_ubo_binding,
			custom_ubo_binding,
			out_particle_ssbo,
		};

		particle_gen_.layout_resource = renderer_->descriptor_allocator_.CreateDescriptorSetLayoutResource(layout_bindings, 0);
		NameObject(context_->device, particle_gen_.layout_resource.layout, "Particle_Compute_Set_Layout");

		// Make descriptor set.
		particle_gen_.descriptor_set_resource = renderer_->descriptor_allocator_.CreateDescriptorSetResource(particle_gen_.layout_resource);
		NameObject(context_->device, particle_gen_.descriptor_set_resource.descriptor_set, "Particle_Compute_Set");

		// Create and link built-in UBO buffer.
		particle_gen_.built_in_ubo_buffer = renderer_->allocator_.CreateBufferResource(
			sizeof(ParticleGenShaderResources::BuiltInUBO),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		NameObject(context_->device, particle_gen_.built_in_ubo_buffer.buffer, "Built_In_UBO_Buffer");
		particle_gen_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_BUILT_IN_UBO_BINDING, particle_gen_.built_in_ubo_buffer);

		// Create and link static particle output buffer.
		VkBufferUsageFlags usage_flags{ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT };
		if (DYNAMIC_PARTICLE_MESH_CPU_BUILD) {
			usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		}
		particle_gen_.particle_out_buffer = renderer_->allocator_.CreateBufferResource(
			sizeof(Voxel) * CHUNK_TOTAL_VOXEL_COUNT,
			usage_flags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, particle_gen_.particle_out_buffer.buffer, "Particle_Out_Buffer");
		particle_gen_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_OUT_PARTICLE_SSBO_BINDING, particle_gen_.particle_out_buffer);

		// Note: custom_ubo_buffer will be made when SetParticleGenShader() is called.
	}

	void ParticleGenContext::InitializeParticleNeighborsShaderResources()
	{
		// Make descriptor set layout.
		VkDescriptorSetLayoutBinding in_particle_ssbo{
			.binding = PARTICLE_NEIGHBOR_IN_PARTICLE_SSBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding out_neighbor_ssbo{
			.binding = PARTICLE_NEIGHBOR_OUT_NEIGHBOR_SSBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> layout_bindings{
			in_particle_ssbo,
			out_neighbor_ssbo,
		};

		particle_neighbors_.layout_resource = renderer_->descriptor_allocator_.CreateDescriptorSetLayoutResource(layout_bindings, 0);
		NameObject(context_->device, particle_neighbors_.layout_resource.layout, "Particle_Neighbor_Set_Layout");

		// Make descriptor set.
		particle_neighbors_.descriptor_set_resource = renderer_->descriptor_allocator_.CreateDescriptorSetResource(particle_neighbors_.layout_resource);
		NameObject(context_->device, particle_neighbors_.descriptor_set_resource.descriptor_set, "Particle_Neighbor_Set");

		// Assign and link particle in-buffer.
		particle_neighbors_.particle_in_buffer = &particle_gen_.particle_out_buffer;
		particle_neighbors_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_NEIGHBOR_IN_PARTICLE_SSBO_BINDING, *particle_neighbors_.particle_in_buffer);

		// Create and link neighbor out-buffer.
		particle_neighbors_.neighbor_out_buffer = renderer_->allocator_.CreateBufferResource(
			sizeof(ParticleSidesFlagBits) * CHUNK_TOTAL_VOXEL_COUNT,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		NameObject(context_->device, particle_neighbors_.neighbor_out_buffer.buffer, "Neighbor_Out_Buffer");
		particle_neighbors_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_NEIGHBOR_OUT_NEIGHBOR_SSBO_BINDING, particle_neighbors_.neighbor_out_buffer);

		// Make the compute pipeline.
		std::vector<DescriptorSetLayoutResource> compute_layouts{
			particle_neighbors_.layout_resource,
		};

		particle_neighbors_.pipeline.Initialize(context_, compute_layouts, {}, SPIRV_PREFIX / "particle_neighbors.comp.spv");
		NameObject(context_->device, particle_neighbors_.pipeline.pipeline, "Particle_Neighbor_Pipeline");
		NameObject(context_->device, particle_neighbors_.pipeline.layout, "Particle_Neighbor_Pipeline_Layout");
	}

	void ParticleGenContext::InitializeParticleMeshShaderResources()
	{
		// Make descriptor set layout.
		VkDescriptorSetLayoutBinding positions_in_ssbo{
			.binding = PARTICLE_MESH_IN_POSITIONS_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding vertices_out_ssbo{
			.binding = PARTICLE_MESH_OUT_VERTICES_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding indices_out_ssbo{
			.binding = PARTICLE_MESH_OUT_INDICES_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding ubo{
			.binding = PARTICLE_MESH_UBO_BINDING,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		std::vector<VkDescriptorSetLayoutBinding> layout_bindings{
			positions_in_ssbo,
			vertices_out_ssbo,
			indices_out_ssbo,
			ubo,
		};

		particle_mesh_.layout_resource = renderer_->descriptor_allocator_.CreateDescriptorSetLayoutResource(layout_bindings, 0);
		NameObject(context_->device, particle_mesh_.layout_resource.layout, "Particle_Mesh_Set_Layout");

		for (ParticleGenContext::FrameResources& frame : frame_resources_)
		{
			// Make descriptor set.
			frame.particle_mesh.descriptor_set_resource = renderer_->descriptor_allocator_.CreateDescriptorSetResource(particle_mesh_.layout_resource);
			NameObject(context_->device, frame.particle_mesh.descriptor_set_resource.descriptor_set, "Particle_Mesh_Set");

			// Create and link UBO buffer.
			frame.particle_mesh.ubo_buffer = renderer_->allocator_.CreateBufferResource(
				sizeof(ParticleMeshFrameShaderResources::UBO),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			NameObject(context_->device, frame.particle_mesh.ubo_buffer.buffer, "Particle_Mesh_Ubo_Buffer");
			frame.particle_mesh.descriptor_set_resource.LinkBufferToBinding(PARTICLE_MESH_UBO_BINDING, frame.particle_mesh.ubo_buffer);
		}

		// Buffers for positions, vertices, and indices will need to be made dynamically later based on the number of particles.

		// Make the compute pipeline.
		std::vector<DescriptorSetLayoutResource> compute_layouts{
			particle_mesh_.layout_resource,
		};

		particle_mesh_.pipeline.Initialize(context_, compute_layouts, {}, SPIRV_PREFIX / "generate_dynamic_particle_mesh.comp.spv");
		NameObject(context_->device, particle_mesh_.pipeline.pipeline, "Particle_Mesh_Pipeline");
		NameObject(context_->device, particle_mesh_.pipeline.layout, "Particle_Mesh_Pipeline_Layout");
	}

	void ParticleGenContext::InitializeCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = renderer_->command_pool_,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		for (FrameResources& resource : frame_resources_)
		{
			VkCommandBuffer* cmd_buffer{ &resource.command_buffer };
			VkResult result{ vkAllocateCommandBuffers(context_->device, &allocate_info, cmd_buffer) };
			CheckResult(result, "Failed to allocate command buffer.");
		}
	}

	void ParticleGenContext::InitializeFences()
	{
		// Start in signaled state since frame resources are initially not in use.
		VkFenceCreateInfo fence_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		for (FrameResources& resource : frame_resources_)
		{
			VkResult result{ vkCreateFence(context_->device, &fence_info, nullptr, &resource.fence) };
			CheckResult(result, "Failed to create fence.");
			NameObject(context_->device, resource.fence, "Particle_Gen_Fence");
		}
	}

	void ParticleGenContext::NextFrame()
	{
		current_frame_ = (current_frame_ + 1) % FRAMES_IN_FLIGHT;
		commands_recorded_ = false;
	}

	ParticleGenContext::FrameResources& ParticleGenContext::GetCurrentFrame()
	{
		return frame_resources_[current_frame_];
	}

	void ParticleGenContext::GenerateDynamicParticleMesh(
		RenderObjectHandle ro_target,
		const std::byte* positions,
		uint32_t offset,
		uint32_t stride,
		const std::vector<MaterialRange>& mat_ranges)
	{
		ZoneScoped;
		mat_ranges_ = mat_ranges;
		Mesh* mesh{ new Mesh{} };
		mesh->geometries.resize(std::max(mat_ranges.size(), (size_t)1));

		for (uint32_t i{ 0 }; i < (uint32_t)mat_ranges.size(); ++i)
		{
			const MaterialRange& mat_offset{ mat_ranges[i] };

			// Generate vertices.
			uint32_t particle_vert_count{};
			{
				ZoneScopedN("Generate vertices");
				std::vector<Vertex> particle_vertices{ GetParticleVertices() };
				particle_vert_count = (uint32_t)particle_vertices.size();
				uint32_t vertex_count{ particle_vert_count * mat_offset.count };
				mesh->geometries[i].vertices.resize(vertex_count);

				for (uint32_t p{ 0 }; p < mat_offset.count; ++p)
				{
					for (uint32_t v{ 0 }; v < particle_vert_count; ++v)
					{
						const glm::vec3& position{ *reinterpret_cast<const glm::vec3*>(positions + (p + mat_offset.offset) * stride + offset) };
						uint32_t vert_buffer_idx{ p * particle_vert_count + v };
						mesh->geometries[i].vertices[vert_buffer_idx] = particle_vertices[v];
						mesh->geometries[i].vertices[vert_buffer_idx].position += glm::vec4{ position, 0.0f };
					}
				}
			}

			// Generate indices.
			{
				ZoneScopedN("Generate indices");
				std::vector<uint32_t> particle_indices{ GetParticleIndices() };
				uint32_t index_count{ (uint32_t)(particle_indices.size() * mat_offset.count) };
				mesh->geometries[i].indices.resize(index_count);

				for (uint32_t p{ 0 }; p < mat_offset.count; ++p)
				{
					for (uint32_t j{ 0 }; j < (uint32_t)particle_indices.size(); ++j)
					{
						uint32_t idx_buffer_idx{ p * (uint32_t)particle_indices.size() + j };
						mesh->geometries[i].indices[idx_buffer_idx] = p * particle_vert_count + particle_indices[j];
					}
				}
			}
		}

		{
			ZoneScopedN("Calculate tangents");
			CalculateTangents(mesh);
		}

		{
			ZoneScopedN("Replace render object");
			renderer_->ReplaceRenderObjectAndBuildBlas(ro_target, mesh);
		}
	}

	void ParticleGenContext::SetPhysicsToRenderMaterialMap(std::vector<int>&& physics_to_render_mat_idx)
	{
		physics_to_render_mat_idx_ = std::move(physics_to_render_mat_idx);
	}

	void ParticleGenContext::UpdatePhysicsRenderMaterials(RenderObjectHandle ro_target)
	{
		// Assign render object's material indices based on physics materials.
		std::vector<int> material_indices{};
		material_indices.resize(mat_ranges_.size());
		std::transform(mat_ranges_.begin(), mat_ranges_.end(), material_indices.begin(),
			[&](const MaterialRange& m) {
#ifdef EDITOR_ENABLED
				// For editor convenience we just use available physics material if enough haven't been created yet.
				uint32_t idx{ std::min(m.physics_material_index, (uint8_t)(physics_to_render_mat_idx_.size() - 1)) };
#else
				uint32_t idx{ m.physics_material_index };
#endif
				return physics_to_render_mat_idx_[idx];
			});
		renderer_->SetRenderObjectMaterialIndices(ro_target, material_indices);
	}

	void ParticleGenContext::CmdBegin()
	{
		ZoneScoped;
		VkResult result{ vkWaitForFences(context_->device, 1, &GetCurrentFrame().fence, VK_TRUE, 1'000'000'000) };
		CheckResult(result, "Error waiting for render_fence.");
		result = vkResetFences(context_->device, 1, &GetCurrentFrame().fence);
		CheckResult(result, "Error resetting render_fence.");
		result = vkResetCommandBuffer(GetCurrentFrame().command_buffer, 0);
		CheckResult(result, "Error resetting command buffer.");


		commands_recorded_ = true;

		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		vkBeginCommandBuffer(GetCurrentFrame().command_buffer, &begin_info);
	}

	void ParticleGenContext::CmdGenerateDynamicParticleMesh(
		RenderObjectHandle ro_target,
		const std::byte* positions,
		uint32_t position_count,
		uint32_t offset,
		uint32_t stride,
		const std::vector<MaterialRange>& mat_ranges)
	{
		mat_ranges_ = mat_ranges;

		VkCommandBuffer& cmd{ GetCurrentFrame().command_buffer };
		const uint32_t position_buffer_size{ stride * position_count };

		constexpr uint32_t CUBE_VERTEX_COUNT{ 24 };
		constexpr uint32_t CUBE_INDEX_COUNT{ 36 };
		bool buffer_expanded{};

		// Create or resize particle in/out buffers if necessary.
		buffer_expanded = renderer_->allocator_.ExpandOrReuseBuffer(
			(uint64_t)position_buffer_size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			GetCurrentFrame().particle_mesh.positions_in);
		if (buffer_expanded)
		{
			NameObject(context_->device, GetCurrentFrame().particle_mesh.positions_in.buffer, "Particle_Mesh_Positions_In");
			GetCurrentFrame().particle_mesh.descriptor_set_resource.LinkBufferToBinding(PARTICLE_MESH_IN_POSITIONS_BINDING, GetCurrentFrame().particle_mesh.positions_in);
		}

		buffer_expanded = renderer_->allocator_.ExpandOrReuseBuffer(
			sizeof(Vertex) * position_count * CUBE_VERTEX_COUNT,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			GetCurrentFrame().particle_mesh.vertices_out);
		if (buffer_expanded)
		{
			NameObject(context_->device, GetCurrentFrame().particle_mesh.vertices_out.buffer, "Particle_Mesh_Vertices_Out");
			GetCurrentFrame().particle_mesh.descriptor_set_resource.LinkBufferToBinding(PARTICLE_MESH_OUT_VERTICES_BINDING, GetCurrentFrame().particle_mesh.vertices_out);
		}

		buffer_expanded = renderer_->allocator_.ExpandOrReuseBuffer(
			sizeof(uint32_t) * position_count * CUBE_INDEX_COUNT,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			GetCurrentFrame().particle_mesh.indices_out);
		if (buffer_expanded)
		{
			NameObject(context_->device, GetCurrentFrame().particle_mesh.indices_out.buffer, "Particle_Mesh_Indices_Out");
			GetCurrentFrame().particle_mesh.descriptor_set_resource.LinkBufferToBinding(PARTICLE_MESH_OUT_INDICES_BINDING, GetCurrentFrame().particle_mesh.indices_out);
		}

		// Populate UBO buffer.
		// TODO: Make the ubo_cpu a member variable and only update the position count every frame.
		ParticleMeshFrameShaderResources::UBO ubo_cpu{
			.cube_vertices = {}, // Set below.
			.cube_indices = {},  // Set below.
			.position_stride_dword = stride / 4, // Convert from bytes to dword.
			.position_offset_dword = offset / 4, // Convert from bytes to dword.
			.position_count = position_count,
		};
		std::vector<Vertex> vertices{ GetParticleVertices() };
		std::vector<uint32_t> indices{ GetParticleIndices() };
		std::copy(vertices.begin(), vertices.end(), ubo_cpu.cube_vertices);
		std::copy(indices.begin(), indices.end(), ubo_cpu.cube_indices);

		BufferResource& ubo_gpu{ GetCurrentFrame().particle_mesh.ubo_buffer };
		void* data{};
		vkMapMemory(context_->device, *ubo_gpu.memory, ubo_gpu.offset, ubo_gpu.size, 0, &data);
		std::memcpy(data, &ubo_cpu, sizeof(ParticleMeshFrameShaderResources::UBO));
		vkUnmapMemory(context_->device, *ubo_gpu.memory);

		// Copy position data to GPU buffer.
		renderer_->vulkan_util_.TransferBufferToDeviceCmd(cmd, positions, position_buffer_size, GetCurrentFrame().particle_mesh.positions_in);
		PipelineBarrier(
			cmd,
			GetCurrentFrame().particle_mesh.positions_in.buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		PipelineBarrier(
			cmd,
			GetCurrentFrame().particle_mesh.ubo_buffer.buffer,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			particle_mesh_.pipeline.layout,
			PARTICLE_MESH_DESCRIPTOR_SET,
			1,
			&GetCurrentFrame().particle_mesh.descriptor_set_resource.descriptor_set,
			0,
			nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, particle_mesh_.pipeline.pipeline);
		constexpr uint32_t group_size{ 64 };
		uint32_t group_count{ (position_count + group_size - 1) / group_size }; // Add (group_size - 1) to get ceil(position_count / groupsize).
		vkCmdDispatch(cmd, group_count, 1, 1);

		PipelineBarrier(
			cmd,
			GetCurrentFrame().particle_mesh.vertices_out.buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);

		PipelineBarrier(
			cmd,
			GetCurrentFrame().particle_mesh.indices_out.buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);

		std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges{};
		std::vector<uint32_t> max_indices{};
		std::vector<uint32_t> index_byte_offsets{};
		{
			build_ranges.reserve(mat_ranges.size());
			max_indices.reserve(mat_ranges.size());
			index_byte_offsets.reserve(mat_ranges.size());
			uint32_t pos_count_accumulator{ 0 };
			for (const MaterialRange& mat_range : mat_ranges)
			{
				uint32_t primitive_offset{ pos_count_accumulator * CUBE_INDEX_COUNT * sizeof(uint32_t) };
				VkAccelerationStructureBuildRangeInfoKHR build_range{
					.primitiveCount = (CUBE_INDEX_COUNT / 3) * mat_range.count,
					.primitiveOffset = primitive_offset, // Byte offset into index buffer.
					.firstVertex = 0, // Zero since index values already index into correct part of vertex buffer.
					.transformOffset = 0,
				};
				build_ranges.push_back(std::move(build_range));
				index_byte_offsets.push_back(primitive_offset);

				pos_count_accumulator += mat_range.count;

				max_indices.push_back(pos_count_accumulator * CUBE_INDEX_COUNT - 1);
			}
		}

		MeshBlasInfo mesh_info{
			.max_indices = std::move(max_indices),
			.build_ranges = std::move(build_ranges),
		};

		Mesh* mesh{ new Mesh{} };
		mesh->geometries.emplace_back();
		mesh->geometries.back().indices_resource = GetCurrentFrame().particle_mesh.indices_out;
		mesh->geometries.back().vertices_resource = GetCurrentFrame().particle_mesh.vertices_out;
		mesh->preserve_geometry_buffers = true; // Old buffers get cleaned up during ExpandOrReuseBuffer().
		mesh->use_single_buffer = true;         // We only use buffers from a single renderer::Geometry that are shared by all Vulkan geometries.
		mesh->index_byte_offsets = std::move(index_byte_offsets);

		renderer_->rt_context_.QueueBlas(mesh, mesh_info);

		// Do not replace render object yet since last frame's resources are still in use. We do it during VulkanRenderer::HostRenderWork().
		renderer_->QueueReplaceRenderObject(ro_target, mesh);
	}

	void ParticleGenContext::CmdSubmit()
	{
		VkCommandBuffer cmd{ GetCurrentFrame().command_buffer };
		vkEndCommandBuffer(cmd);

		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr,
		};

		VkResult result{ vkQueueSubmit(renderer_->context_.graphics_queue, 1, &submit_info, GetCurrentFrame().fence) };
		CheckResult(result, "Error submitting compute command buffer.");
	}

	bool ParticleGenContext::CommandsRecordedThisFrame()
	{
		return commands_recorded_;
	}

	std::vector<MaterialPosition> VoxelChunkToMaterialPositions(const VoxelChunk& voxel_chunk, const glm::vec3& object_origin)
	{
		std::vector<MaterialPosition> mat_positions{};
		for (uint32_t i{ 0 }; i < voxel_chunk.VoxelCount(); ++i)
		{
			if (voxel_chunk.IsEmpty(i) || voxel_chunk.IsOccluded(i)) {
				continue;
			}

			MaterialPosition mat_position{
				.physics_material_index = voxel_chunk.Index(i).physics_material_index,
				.position = PARTICLE_WIDTH * glm::vec3(voxel_chunk.IndexToCoordinate(i)) - object_origin,
			};
			mat_positions.push_back(mat_position);
		}
		return mat_positions;
	}

	void ParticleGenContext::GenerateStaticParticleMesh(RenderObjectHandle ro_target, const VoxelChunk& voxel_chunk, const glm::vec3& object_origin)
	{
		// When true, forces to always generate dynamic particle meshes for debugging purposes.
		if (DISABLE_STATIC_PARTICLE_MESH)
		{
			std::vector<MaterialPosition> mat_positions{ VoxelChunkToMaterialPositions(voxel_chunk, object_origin) };

			std::sort(mat_positions.begin(), mat_positions.end(),
				[](const MaterialPosition& p0, const MaterialPosition& p1) { return p0.physics_material_index < p1.physics_material_index; });

			mat_ranges_ = CreateMaterialRanges(mat_positions);
			GenerateDynamicParticleMesh(ro_target, (const std::byte*)mat_positions.data(), offsetof(MaterialPosition, position), sizeof(MaterialPosition), mat_ranges_);

			UpdatePhysicsRenderMaterials(ro_target);
			return;
		}

		// TODO: Experiment and measure speed with reinterpretting particles as uint64_t.
		//constexpr uint64_t empty_particles{ 0 };
		//constexpr uint64_t all_sides{ 0x3F3f3f3f3F3f3f3f };

		StaticParticleMeshGenerator gen{};
		Mesh* mesh{ gen.Generate(voxel_chunk) };
		renderer_->ReplaceRenderObject(ro_target, mesh);
	}

	std::vector<Vertex> ParticleGenContext::GetParticleVertices() const
	{
		uint32_t vert_count{ 24 }; // Cube with 3 normals per corner so 8 * 3 vertices.
		std::vector<Vertex> verts(vert_count);
		float particle_radius{ PARTICLE_WIDTH / 2.0f };

		// Position. Three of each since there are three different normals at each corner.
		verts[0].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[1].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[2].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[3].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[4].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[5].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[6].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[7].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[8].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[9].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[10].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[11].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;

		verts[12].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[13].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[14].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[15].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[16].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[17].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[18].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[19].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[20].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[21].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[22].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[23].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;

		// Normals.
		verts[0].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f };  // 0 vertex.
		verts[1].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f };  // 0 vertex.
		verts[2].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };   // 0 vertex.
		verts[3].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f };  // 1 vertex.
		verts[4].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f };  // 1 vertex.
		verts[5].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f };  // 1 vertex.
		verts[6].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f };  // 2 vertex.
		verts[7].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };   // 2 vertex.
		verts[8].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };   // 2 vertex.
		verts[9].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f };  // 3 vertex.
		verts[10].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 3 vertex.
		verts[11].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 3 vertex.

		verts[12].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 4 vertex.
		verts[13].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 4 vertex.
		verts[14].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };  // 4 vertex.
		verts[15].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 5 vertex.
		verts[16].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 5 vertex.
		verts[17].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 5 vertex.
		verts[18].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 6 vertex.
		verts[19].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 6 vertex.
		verts[20].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };  // 6 vertex.
		verts[21].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 7 vertex.
		verts[22].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 7 vertex.
		verts[23].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 7 vertex.

		return verts;
	}

	std::vector<uint32_t> ParticleGenContext::GetParticleIndices() const
	{
		// Seems that Vulkan RT API has counter clockwise hardcoded as front face.
		return {
			8, 2, 20,   // Z- plane.
			20, 2, 14,  // Z- plane.
			3, 0, 6,    // X- plane.
			9, 3, 6,    // X- plane.
			23, 5, 11,  // Z+ plane.
			17, 5, 23,  // Z+ plane.
			18, 15, 21, // X+ plane.
			12, 15, 18, // X+ plane.
			7, 19, 10,  // Y+ plane.
			10, 19, 22, // Y+ plane.
			13, 1, 4,   // Y- plane.
			16, 13, 4,  // Y- plane.
		};
	}

	Mesh* StaticParticleMeshGenerator::Generate(const VoxelChunk& voxel_chunk)
	{
		/*
		mesh_ = new Mesh{};
		mesh_->geometries.emplace_back();

		GenerateSide(ParticleSidesFlagBits::X_POSITIVE, particles, side_flags);
		GenerateSide(ParticleSidesFlagBits::Y_POSITIVE, particles, side_flags);
		GenerateSide(ParticleSidesFlagBits::Z_POSITIVE, particles, side_flags);

		CalculateTangents(mesh_);
		return mesh_;
		*/
		return nullptr;
	}

	void StaticParticleMeshGenerator::GenerateSide(ParticleSidesFlagBits side, const std::vector<Voxel>& particles, const std::vector<uint8_t>& side_flags)
	{
		/*
		rectangle_indices_.resize(CHUNK_ROW_VOXEL_COUNT, NULL_INDEX);
		uint32_t rect_start{}; // Coordinate of start of current rectangle.

		// Abstract x_, y_, z_ coordinates so algorithm can be done for any sides of the voxels.
		uint32_t& h{ GetHorizontalReference(side) };
		uint32_t& v{ GetVerticalReference(side) };
		uint32_t& d{ GetDepthReference(side) };

		for (d = 0; d < CHUNK_ROW_VOXEL_COUNT; ++d)
		{
			for (v = 0; v < CHUNK_ROW_VOXEL_COUNT; ++v)
			{
				rect_start = NULL_INDEX;
				for (h = 0; h < CHUNK_ROW_VOXEL_COUNT; ++h)
				{
					uint32_t i{ CoordinateToParticleIndex({x_, y_, z_}) };

					uint32_t current_rect_idx = rectangle_indices_[h];
					bool in_rectangle_domain{ current_rect_idx != NULL_INDEX };
					bool solid_particle{ particles[i].physics_material_index != PHYSICS_MATERIAL_EMPTY_INDEX };
					bool occluded{ (bool)(side_flags[i] & (uint8_t)side) };
					bool part_of_shell{ solid_particle && !occluded }; // Each solid and non-occluded face will be part of a the triangle shell.
					bool end_of_row{ h == CHUNK_ROW_VOXEL_COUNT - 1 };

					if (in_rectangle_domain)
					{
						if (part_of_shell)
						{
							// If null then we just entered a rectangle, so we mark the start of it.
							if (rect_start == NULL_INDEX) {
								rect_start = h;
							}
							// Otherwise if we reached the end of the rectangle, then we clear the start marker.
							else if (h == rectangles_[current_rect_idx].end_h) {
								rect_start = NULL_INDEX;
							}
						}
						else
						{
							// In checking to see if a rectangle can get another row, we found empty block so rectangle is done.
							TriangulateRectangle(side, current_rect_idx, v, d);
							ClearRectangleIndices(current_rect_idx);
						}
					}
					else
					{
						// Just entered a part of shell, so mark start of it.
						if (part_of_shell && (rect_start == NULL_INDEX)) {
							rect_start = h;
						}
						// Otherwise we made it to the end of a streak of shell blocks, so create a new rectangle if it's not already a part of one.
						else if ((!part_of_shell || end_of_row) && (rect_start != NULL_INDEX) && (rectangle_indices_[rect_start] == NULL_INDEX))
						{
							Rectangle rect{
								.start_h = rect_start,
								.end_h = end_of_row ? h : h - 1,
								.start_v = v,
							};
							uint32_t rect_idx{ (uint32_t)rectangles_.size() };
							rectangles_.push_back(rect);
							SetRectangleIndices(rect_idx);
							rect_start = NULL_INDEX;
						}
					}
				}
			}

			// Just finished slice, so triangulate all of the remaining rectangles.
			for (uint32_t rect_idx : rectangle_indices_)
			{
				if (rect_idx != NULL_INDEX) {
					TriangulateRectangle(side, rect_idx, CHUNK_ROW_VOXEL_COUNT - 1, CHUNK_ROW_VOXEL_COUNT - 1);
					ClearRectangleIndices(rect_idx);
				}
			}
			rectangles_.clear();
		}
		*/
	}

	void StaticParticleMeshGenerator::TriangulateRectangle(ParticleSidesFlagBits side, uint32_t rect_idx, uint32_t vertical, uint32_t depth)
	{
		float left{ (float)rectangles_[rect_idx].start_h };
		float right{ (float)rectangles_[rect_idx].end_h + 1 }; // Add one since triangle ends at rightmost edge of the block.
		float top{ (float)vertical };
		float bottom{ (float)rectangles_[rect_idx].start_v };

		Vertex top_left{};
		Vertex top_right{};
		Vertex bottom_right{};
		Vertex bottom_left{};
		bool switch_winding_order{}; // Need to switch winding order if z-coordinate is horizontal or vertical.
		switch (side)
		{
		case ParticleSidesFlagBits::X_POSITIVE:
			++depth;
			[[fallthrough]];
		case ParticleSidesFlagBits::X_NEGATIVE:
			top_left = { {     (float)depth, top,    left,  0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			top_right = { {    (float)depth, top,    right, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			bottom_right = { { (float)depth, bottom, right, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			bottom_left = { {  (float)depth, bottom, left,  0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			switch_winding_order = true;
			break;
		case ParticleSidesFlagBits::Y_POSITIVE:
			++depth;
			[[fallthrough]];
		case ParticleSidesFlagBits::Y_NEGATIVE:
			top_left = { {     left,  (float)depth, top,    0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			top_right = { {    right, (float)depth, top,    0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			bottom_right = { { right, (float)depth, bottom, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			bottom_left = { {  left,  (float)depth, bottom, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			switch_winding_order = true;
			break;
		case ParticleSidesFlagBits::Z_POSITIVE:
			++depth;
			[[fallthrough]];
		case ParticleSidesFlagBits::Z_NEGATIVE:
			top_left = { {     left,  top,    (float)depth, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f} };
			top_right = { {    right, top,    (float)depth, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f} };
			bottom_right = { { right, bottom, (float)depth, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f} };
			bottom_left = { {  left,  bottom, (float)depth, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f} };
			switch_winding_order = false;
			break;
		}

		uint32_t start_index{ (uint32_t)mesh_->geometries[0].vertices.size() };

		mesh_->geometries[0].vertices.push_back(top_left);
		mesh_->geometries[0].vertices.push_back(top_right);
		mesh_->geometries[0].vertices.push_back(bottom_right);
		mesh_->geometries[0].vertices.push_back(bottom_left);

		// Seems that Vulkan RT API has counter clockwise hardcoded as front face.
		std::vector<uint32_t> indices = switch_winding_order ? std::vector<uint32_t>{ 0, 1, 3, 2, 3, 1 } : std::vector<uint32_t>{ 1, 0, 3, 3, 2, 1 };
		for (uint32_t i : indices) {
			mesh_->geometries[0].indices.push_back(start_index + i);
		}
	}

	void StaticParticleMeshGenerator::ClearRectangleIndices(uint32_t rect_idx)
	{
		for (uint32_t i{ rectangles_[rect_idx].start_h }; i <= rectangles_[rect_idx].end_h; ++i)
		{
			if (rectangle_indices_[i] == rect_idx) {
				rectangle_indices_[i] = NULL_INDEX;
			}
		}
	}

	void StaticParticleMeshGenerator::SetRectangleIndices(uint32_t rect_idx)
	{
		for (uint32_t i{ rectangles_[rect_idx].start_h }; i <= rectangles_[rect_idx].end_h; ++i) {
			rectangle_indices_[i] = rect_idx;
		}
	}

	uint32_t& StaticParticleMeshGenerator::GetHorizontalReference(ParticleSidesFlagBits side)
	{
		switch (side)
		{
		case ParticleSidesFlagBits::X_POSITIVE:
			return z_;
		case ParticleSidesFlagBits::X_NEGATIVE:
			return z_;
		case ParticleSidesFlagBits::Y_POSITIVE:
			return x_;
		case ParticleSidesFlagBits::Y_NEGATIVE:
			return x_;
		case ParticleSidesFlagBits::Z_POSITIVE:
			return x_;
		case ParticleSidesFlagBits::Z_NEGATIVE:
			return x_;
		}

		logger::Error("Failed to get horizontal reference.\n");
		return x_;
	}

	uint32_t& StaticParticleMeshGenerator::GetVerticalReference(ParticleSidesFlagBits side)
	{
		switch (side)
		{
		case ParticleSidesFlagBits::X_POSITIVE:
			return y_;
		case ParticleSidesFlagBits::X_NEGATIVE:
			return y_;
		case ParticleSidesFlagBits::Y_POSITIVE:
			return z_;
		case ParticleSidesFlagBits::Y_NEGATIVE:
			return z_;
		case ParticleSidesFlagBits::Z_POSITIVE:
			return y_;
		case ParticleSidesFlagBits::Z_NEGATIVE:
			return y_;
		}

		logger::Error("Failed to get vertical reference.\n");
		return x_;
	}

	uint32_t& StaticParticleMeshGenerator::GetDepthReference(ParticleSidesFlagBits side)
	{
		switch (side)
		{
		case ParticleSidesFlagBits::X_POSITIVE:
			return x_;
		case ParticleSidesFlagBits::X_NEGATIVE:
			return x_;
		case ParticleSidesFlagBits::Y_POSITIVE:
			return y_;
		case ParticleSidesFlagBits::Y_NEGATIVE:
			return y_;
		case ParticleSidesFlagBits::Z_POSITIVE:
			return z_;
		case ParticleSidesFlagBits::Z_NEGATIVE:
			return z_;
		}

		logger::Error("Failed to get depth reference.\n");
		return x_;
	}
}
