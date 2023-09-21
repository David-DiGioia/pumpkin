#include "particles.h"

#include "vulkan_renderer.h"
#include "vulkan_util.h"

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

	glm::uvec3 ParticleIndexToCoordinate(uint32_t index)
	{
		uint32_t slice_area{ PARTICLE_CHUNK_SIZE * PARTICLE_CHUNK_SIZE };
		uint32_t z{ index / slice_area };
		uint32_t y{ (index % slice_area) / PARTICLE_CHUNK_SIZE };
		uint32_t x{ index % PARTICLE_CHUNK_SIZE };

		return glm::uvec3{ x, y, z };
	}

	void ParticleContext::Initialize(Context* context, VulkanRenderer* renderer)
	{
		context_ = context;
		renderer_ = renderer;

		InitializeParticleGenShaderResources();
		InitializeParticleNeighborsShaderResources();
	}

	void ParticleContext::CleanUp()
	{
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.built_in_ubo_buffer);
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.particle_out_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_gen_.layout_resource);

		renderer_->allocator_.DestroyBufferResource(&particle_neighbors_.neighbor_out_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_neighbors_.layout_resource);
		particle_neighbors_.pipeline.CleanUp();
	}

	RenderObjectHandle ParticleContext::InvokeParticleGenShader()
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
		// Calculate static particles.
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

		std::vector<StaticParticle> static_particles(PARTICLE_CHUNK_VOLUME);
		renderer_->vulkan_util_.TransferBufferToHost(static_particles, particle_gen_.particle_out_buffer);
		renderer_->vulkan_util_.Submit();

		// Copy the particle neighbor data to a vector.
		std::vector<uint8_t> side_flags(PARTICLE_CHUNK_VOLUME);
		vkMapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory, particle_neighbors_.neighbor_out_buffer.offset, particle_neighbors_.neighbor_out_buffer.size, 0, &data);
		std::memcpy(side_flags.data(), data, sizeof(uint8_t) * PARTICLE_CHUNK_VOLUME);
		vkUnmapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory);

		constexpr float particle_size{ 0.1f };
		return GenerateStaticParticleMesh(static_particles, side_flags, particle_size);
	}

	void ParticleContext::SetParticleGenShader(uint32_t shader_idx, const std::vector<std::byte>& custom_ubo_buffer)
	{
		particle_gen_.shader_idx = shader_idx;

		if (particle_gen_.custom_ubo_buffer.buffer != VK_NULL_HANDLE)
		{
			renderer_->allocator_.DestroyBufferResource(&particle_gen_.custom_ubo_buffer);
			particle_gen_.custom_ubo_buffer.buffer = VK_NULL_HANDLE;
		}

		particle_gen_.custom_ubo_buffer = renderer_->allocator_.CreateBufferResource(
			(VkDeviceSize)custom_ubo_buffer.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		particle_gen_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_CUSTOM_UBO_BINDING, particle_gen_.custom_ubo_buffer);
	}

	DescriptorSetLayoutResource& ParticleContext::GetParticleGenLayoutResource()
	{
		return particle_gen_.layout_resource;
	}

	void ParticleContext::InitializeParticleGenShaderResources()
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
			sizeof(StaticParticle) * PARTICLE_CHUNK_VOLUME,
			usage_flags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		NameObject(context_->device, particle_gen_.particle_out_buffer.buffer, "Particle_Out_Buffer");
		particle_gen_.descriptor_set_resource.LinkBufferToBinding(PARTICLE_OUT_PARTICLE_SSBO_BINDING, particle_gen_.particle_out_buffer);

		// Note: custom_ubo_buffer will be made when SetParticleGenShader() is called.
	}

	void ParticleContext::InitializeParticleNeighborsShaderResources()
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
			sizeof(ParticleSidesFlagBits) * PARTICLE_CHUNK_VOLUME,
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

	RenderObjectHandle ParticleContext::GenerateDynamicParticleMesh(const std::vector<Particle>& particles, float particle_width)
	{
		Mesh* mesh{ new Mesh{} };
		mesh->geometries.resize(1);

		// Generate vertices.
		uint32_t particle_vert_count{};
		{
			std::vector<Vertex> particle_vertices{ GetParticleVertices(particle_width) };
			particle_vert_count = (uint32_t)particle_vertices.size();
			uint32_t vertex_count{ (uint32_t)(particle_vert_count * particles.size()) };
			mesh->geometries[0].vertices.resize(vertex_count);

			for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
			{
				for (uint32_t v{ 0 }; v < particle_vert_count; ++v)
				{
					uint32_t vert_buffer_idx{ p * particle_vert_count + v };
					mesh->geometries[0].vertices[vert_buffer_idx] = particle_vertices[v];
					mesh->geometries[0].vertices[vert_buffer_idx].position += glm::vec4{ particles[p].position, 0.0f };
				}
			}
		}

		// Generate indices.
		{
			std::vector<uint32_t> particle_indices{ GetParticleIndices() };
			uint32_t index_count{ (uint32_t)(particle_indices.size() * particles.size()) };
			mesh->geometries[0].indices.resize(index_count);

			for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
			{
				for (uint32_t i{ 0 }; i < (uint32_t)particle_indices.size(); ++i)
				{
					uint32_t idx_buffer_idx{ p * (uint32_t)particle_indices.size() + i };
					mesh->geometries[0].indices[idx_buffer_idx] = p * particle_vert_count + particle_indices[i];
				}
			}
		}

		CalculateTangents(mesh);
		return renderer_->CreateRenderObjectFromMesh(mesh, { 0 });
	}


	RenderObjectHandle ParticleContext::GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags, float particle_width)
	{
		// When enabled, forces to always generate dynamic particle meshes for debugging purposes.
		if (DISABLE_STATIC_PARTICLE_MESH) {
			return GenerateDynamicParticleMesh(StaticParticleToDynamic(particles, side_flags, particle_width), particle_width);
		}

		// TODO: Experiment and measure speed with reinterpretting particles as uint64_t.
		//constexpr uint64_t empty_particles{ 0 };
		//constexpr uint64_t all_sides{ 0x3F3f3f3f3F3f3f3f };

		StaticParticleMeshGenerator gen{};
		Mesh* mesh{ gen.Generate(particles, side_flags, particle_width) };
		return renderer_->CreateRenderObjectFromMesh(mesh, { 0 });
	}

	std::vector<Vertex> ParticleContext::GetParticleVertices(float particle_width) const
	{
		uint32_t vert_count{ 24 }; // Cube with 3 normals per corner so 8 * 3 vertices.
		std::vector<Vertex> verts(vert_count);
		float particle_radius{ particle_width / 2.0f };

		// Position. Three of each since there are three different normals at each corner.
		verts[0].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[1].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[2].position = glm::vec4{ -1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[3].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[4].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[5].position = glm::vec4{ -1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[6].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[7].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[8].position = glm::vec4{ -1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[9].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[10].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[11].position = glm::vec4{ -1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;

		verts[12].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[13].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[14].position = glm::vec4{ 1.0f, -1.0f, -1.0f, 0.0f } *particle_radius;
		verts[15].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[16].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[17].position = glm::vec4{ 1.0f, -1.0f, 1.0f, 0.0f } *particle_radius;
		verts[18].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[19].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[20].position = glm::vec4{ 1.0f, 1.0f, -1.0f, 0.0f } *particle_radius;
		verts[21].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[22].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;
		verts[23].position = glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f } *particle_radius;

		// Normals.
		verts[0].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }; // 0 vertex.
		verts[1].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 0 vertex.
		verts[2].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 0 vertex.
		verts[3].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }; // 1 vertex.
		verts[4].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 1 vertex.
		verts[5].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };  // 1 vertex.
		verts[6].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }; // 2 vertex.
		verts[7].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 2 vertex.
		verts[8].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 2 vertex.
		verts[9].normal = glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }; // 3 vertex.
		verts[10].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f }; // 3 vertex.
		verts[11].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f }; // 3 vertex.

		verts[12].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 4 vertex.
		verts[13].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 4 vertex.
		verts[14].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 4 vertex.
		verts[15].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 5 vertex.
		verts[16].normal = glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f }; // 5 vertex.
		verts[17].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };  // 5 vertex.
		verts[18].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 6 vertex.
		verts[19].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 6 vertex.
		verts[20].normal = glm::vec4{ 0.0f, 0.0f, -1.0f, 0.0f }; // 6 vertex.
		verts[21].normal = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };  // 7 vertex.
		verts[22].normal = glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f };  // 7 vertex.
		verts[23].normal = glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };  // 7 vertex.

		return verts;

	}

	std::vector<uint32_t> ParticleContext::GetParticleIndices() const
	{
		return {
			2, 8, 20,   // Z- plane.
			2, 20, 14,  // Z- plane.
			0, 3, 6,    // X- plane.
			3, 9, 6,    // X- plane.
			5, 23, 11,  // Z+ plane.
			5, 17, 23,  // Z+ plane.
			15, 18, 21, // X+ plane.
			15, 12, 18, // X+ plane.
			19, 7, 10,  // Y+ plane.
			19, 10, 22, // Y+ plane.
			1, 13, 4,   // Y- plane.
			13, 16, 4,  // Y- plane.
		};
	}

	std::vector<Particle> ParticleContext::StaticParticleToDynamic(const std::vector<StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags, float particle_width) const
	{
		std::vector<Particle> dynamic_particles{};
		for (uint32_t i{ 0 }; i < PARTICLE_CHUNK_VOLUME; ++i)
		{
			bool empty{ static_particles[i].type == ParticleType::EMPTY };
			bool occluded{ side_flags[i] == (uint8_t)ParticleSidesFlagBits::ALL_SIDES };

			if (empty || occluded) {
				continue;
			}

			Particle particle{
				.position = particle_width * glm::vec3(ParticleIndexToCoordinate(i)),
				.geometry_index = 0,
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	Mesh* StaticParticleMeshGenerator::Generate(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags, float particle_width)
	{
		mesh_ = new Mesh{};
		mesh_->geometries.emplace_back();
		rectangle_indices_.resize(PARTICLE_CHUNK_SIZE, NULL_INDEX);
		uint32_t rect_start{}; // Coordinate of start of current rectangle.

		for (uint32_t i{ 0 }; i < (uint32_t)particles.size(); ++i)
		{
			glm::uvec3 coord{ ParticleIndexToCoordinate(i) };

			if (coord.x == 0) {
				rect_start = NULL_INDEX;
			}

			if (coord.z == PARTICLE_CHUNK_SIZE - 1) {
				uint32_t a = 1;
			}

			uint32_t current_rect_idx = rectangle_indices_[coord.x];
			bool in_rectangle_domain{ current_rect_idx != NULL_INDEX };
			bool solid_particle{ particles[i].type != ParticleType::EMPTY };
			bool occluded{ (bool)(side_flags[i] & (uint8_t)ParticleSidesFlagBits::Z_POSITIVE) };
			bool part_of_shell{ solid_particle && !occluded }; // Each solid and non-occluded face will be part of a the triangle shell.
			bool end_of_row{ coord.x == PARTICLE_CHUNK_SIZE - 1 };

			if (in_rectangle_domain)
			{
				if (part_of_shell)
				{
					// If null then we just entered a rectangle, so we mark the start of it.
					if (rect_start == NULL_INDEX) {
						rect_start = coord.x;
					}
					// Otherwise if we reached the end of the rectangle, then we clear the start marker.
					else if (coord.x == rectangles_[current_rect_idx].end_x) {
						rect_start = NULL_INDEX;
					}
				}
				else
				{
					// In checking to see if a rectangle can get another row, we found empty block so rectangle is done.
					TriangulateRectangle(current_rect_idx, coord);
					ClearRectangleIndices(current_rect_idx, rectangles_[current_rect_idx]);
				}
			}
			else
			{
				// Just entered a part of shell, so mark start of it.
				if (part_of_shell && (rect_start == NULL_INDEX)) {
					rect_start = coord.x;
				}
				// Otherwise we made it to the end of a streak of shell blocks, so create a new rectangle.
				else if ((!part_of_shell || end_of_row) && (rect_start != NULL_INDEX))
				{
					Rectangle rect{
						.start_x = rect_start,
						.end_x = coord.x - 1,
						.start_y = coord.y,
					};
					uint32_t rect_idx{ (uint32_t)rectangles_.size() };
					rectangles_.push_back(rect);
					SetRectangleIndices(rect_idx, rect);
					rect_start = NULL_INDEX;
				}
			}
		}

		for (uint32_t rect_idx{ 0 }; rect_idx < (uint32_t)rectangles_.size(); ++rect_idx)
		{
			if (!rectangles_[rect_idx].trianglulated) {
				TriangulateRectangle(rect_idx, { PARTICLE_CHUNK_SIZE - 1, PARTICLE_CHUNK_SIZE - 1, PARTICLE_CHUNK_SIZE - 1 });
			}
		}

		CalculateTangents(mesh_);
		return mesh_;
	}

	void StaticParticleMeshGenerator::TriangulateRectangle(uint32_t rect_idx, const glm::uvec3& coord)
	{
		float left{ (float)rectangles_[rect_idx].start_x };
		float right{ (float)rectangles_[rect_idx].end_x + 1 }; // Add one since triangle ends at rightmost edge of the block.
		float top{ (float)coord.y };
		float bottom{ (float)rectangles_[rect_idx].start_y };

		Vertex top_left{
			.position = {left, top, (float)coord.z, 0.0f},
			.normal = {0.0f, 0.0f, 1.0f, 0.0f},
		};

		Vertex top_right{
			.position = {right, top, (float)coord.z, 0.0f},
			.normal = {0.0f, 0.0f, 1.0f, 0.0f},
		};

		Vertex bottom_right{
			.position = {right, bottom, (float)coord.z, 0.0f},
			.normal = {0.0f, 0.0f, 1.0f, 0.0f},
		};

		Vertex bottom_left{
			.position = {left, bottom, (float)coord.z, 0.0f},
			.normal = {0.0f, 0.0f, 1.0f, 0.0f},
		};

		uint32_t prev_index{ (uint32_t)mesh_->geometries[0].vertices.size() };

		mesh_->geometries[0].vertices.push_back(top_left);
		mesh_->geometries[0].vertices.push_back(top_right);
		mesh_->geometries[0].vertices.push_back(bottom_right);
		mesh_->geometries[0].vertices.push_back(bottom_left);

		mesh_->geometries[0].indices.push_back(prev_index + 0);
		mesh_->geometries[0].indices.push_back(prev_index + 1);
		mesh_->geometries[0].indices.push_back(prev_index + 3);
		mesh_->geometries[0].indices.push_back(prev_index + 1);
		mesh_->geometries[0].indices.push_back(prev_index + 2);
		mesh_->geometries[0].indices.push_back(prev_index + 3);

		rectangles_[rect_idx].trianglulated = true;
	}

	void StaticParticleMeshGenerator::ClearRectangleIndices(uint32_t rect_idx, const Rectangle& rectangle)
	{
		for (uint32_t i{ rectangle.start_x }; i <= rectangle.end_x; ++i)
		{
			if (rectangle_indices_[i] == rect_idx) {
				rectangle_indices_[i] = NULL_INDEX;
			}
		}
	}

	void StaticParticleMeshGenerator::SetRectangleIndices(uint32_t rect_idx, const Rectangle& rectangle)
	{
		for (uint32_t i{ rectangle.start_x }; i <= rectangle.end_x; ++i) {
			rectangle_indices_[i] = rect_idx;
		}
	}
}
