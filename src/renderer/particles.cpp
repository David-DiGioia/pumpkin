#include "particles.h"

#include "vulkan_renderer.h"
#include "vulkan_util.h"
#include "renderer_constants.h"

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
		uint32_t slice_area{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
		uint32_t z{ index / slice_area };
		uint32_t y{ (index % slice_area) / CHUNK_ROW_VOXEL_COUNT };
		uint32_t x{ index % CHUNK_ROW_VOXEL_COUNT };

		return glm::uvec3{ x, y, z };
	}

	uint32_t CoordinateToParticleIndex(const glm::uvec3& coord)
	{
		uint32_t slice_area{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
		return coord.x + coord.y * CHUNK_ROW_VOXEL_COUNT + coord.z * slice_area;
	}

	void ParticleContext::Initialize(Context* context, VulkanRenderer* renderer)
	{
		context_ = context;
		renderer_ = renderer;

		InitializeParticleGenShaderResources();
		InitializeParticleNeighborsShaderResources();
	}

	void ParticleContext::PhysicsUpdate(float delta_time)
	{
		if (!update_physics_) {
			return;
		}

		mpm_context_.SimulateStep(delta_time);
		GenerateDynamicParticleMesh(mpm_context_.GetParticles());
	}

	void ParticleContext::CleanUp()
	{
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.built_in_ubo_buffer);
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.particle_out_buffer);
		renderer_->allocator_.DestroyBufferResource(&particle_gen_.custom_ubo_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_gen_.layout_resource);

		renderer_->allocator_.DestroyBufferResource(&particle_neighbors_.neighbor_out_buffer);
		renderer_->descriptor_allocator_.DestroyDescriptorSetLayoutResource(&particle_neighbors_.layout_resource);
		particle_neighbors_.pipeline.CleanUp();
	}

	void ParticleContext::InvokeParticleGenShader()
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

		static_particles_.resize(CHUNK_TOTAL_VOXEL_COUNT);
		renderer_->vulkan_util_.TransferBufferToHost(static_particles_, particle_gen_.particle_out_buffer);
		renderer_->vulkan_util_.Submit();

		// Copy the particle neighbor data to a vector.
		side_flags_.resize(CHUNK_TOTAL_VOXEL_COUNT);
		vkMapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory, particle_neighbors_.neighbor_out_buffer.offset, particle_neighbors_.neighbor_out_buffer.size, 0, &data);
		std::memcpy(side_flags_.data(), data, sizeof(uint8_t) * CHUNK_TOTAL_VOXEL_COUNT);
		vkUnmapMemory(context_->device, *particle_neighbors_.neighbor_out_buffer.memory);

		GenerateStaticParticleMesh(static_particles_, side_flags_);
	}

	void ParticleContext::GenerateTestParticle()
	{
		constexpr float youngs_modulus{ 800.0f };
		constexpr float poissons_ratio{ 0.4f };
		float mu{ CalculateMu(youngs_modulus, poissons_ratio) };
		float lambda{ CalculateLambda(youngs_modulus, poissons_ratio) };

		MaterialPoint mpm_particle{
			.mass = .01f,
			.mu = mu,
			.lambda = lambda,
			.position = glm::vec3{0.321932f, 0.452119f, 0.434341f},
			.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
			.affine_matrix = glm::mat3{1.0f},
			.deformation_gradient = glm::mat3{1.0f},
		};

		std::vector<MaterialPoint> mpm_particles{ mpm_particle };
		mpm_context_.Initialize(std::move(mpm_particles), chunk_width_);

		// Since we don't use static particles here, we just simulate a single set to get all the MPM
		// particle info set that needs to be set.
		update_physics_ = true;
		has_played_ = true;
		PhysicsUpdate(1.0f / 60.0f);
		update_physics_ = false;
	}

	void ParticleContext::SetParticleGenShader(uint32_t shader_idx, uint32_t custom_ubo_size)
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

	void ParticleContext::UpdateParticleGenShaderCustomUBO(const std::vector<std::byte>& custom_ubo)
	{
		renderer_->vulkan_util_.Begin();
		renderer_->vulkan_util_.TransferBufferToDevice(custom_ubo, particle_gen_.custom_ubo_buffer);
		renderer_->vulkan_util_.Submit();
	}

	DescriptorSetLayoutResource& ParticleContext::GetParticleGenLayoutResource()
	{
		return particle_gen_.layout_resource;
	}

	void ParticleContext::EnablePhysicsUpdate()
	{
		if (!has_played_)
		{
			has_played_ = true;
			TransferStaticParticlesToMPM();
		}
		update_physics_ = true;
	}

	void ParticleContext::DisablePhysicsUpdate()
	{
		update_physics_ = false;
	}

	void ParticleContext::ResetParticles()
	{
		has_played_ = false;
		DisablePhysicsUpdate();
		TransferStaticParticlesToMPM();
		if (!update_physics_) {
			GenerateDynamicParticleMesh(mpm_context_.GetParticles());
		}
	}

	bool ParticleContext::GetPhysicsUpdateEnabled() const
	{
		return update_physics_;
	}

	bool ParticleContext::GetParticlesEmpty() const
	{
		return static_particles_.empty();
	}

	void ParticleContext::TransferStaticParticlesToMPM()
	{
		std::vector<MaterialPoint> mpm_particles{};

		constexpr float youngs_modulus{ 800.0f };
		constexpr float poissons_ratio{ 0.4f };
		float mu{ CalculateMu(youngs_modulus, poissons_ratio) };
		float lambda{ CalculateLambda(youngs_modulus, poissons_ratio) };

		const float particle_width{ chunk_width_ / CHUNK_ROW_VOXEL_COUNT };

		for (uint32_t i{ 0 }; i < (uint32_t)static_particles_.size(); ++i)
		{
			if (static_particles_[i].type == ParticleType::EMPTY) {
				continue;
			}

			glm::uvec3 coord{ ParticleIndexToCoordinate(i) };

			MaterialPoint mpm_particle{
				.mass = .01f,
				.mu = mu,
				.lambda = lambda,
				.position = particle_width * glm::vec3{coord},
				.velocity = glm::vec3{0.0f, -2.0f, 0.0f},
				.affine_matrix = glm::mat3{1.0f},
				.deformation_gradient = glm::mat3{1.0f},
			};

			mpm_particles.push_back(mpm_particle);
		}

		mpm_context_.Initialize(std::move(mpm_particles), chunk_width_);
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
			sizeof(StaticParticle) * CHUNK_TOTAL_VOXEL_COUNT,
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

	void ParticleContext::SetTargetRenderObject(RenderObjectHandle ro_target)
	{
		ro_target_ = ro_target;
	}

	void ParticleContext::SetChunkWidth(float chunk_width)
	{
		chunk_width_ = chunk_width;
	}

	float ParticleContext::GetChunkWidth() const
	{
		return chunk_width_;
	}

#ifdef EDITOR_ENABLED
	void ParticleContext::SetMPMDebugParticleGenEnabled(bool enabled)
	{
		generate_mpm_particle_instances_ = enabled;

		// If we've already generated a dynamic particle mesh before enabling,
		// we won't have the MPM debug mesh when we need it.
		if (enabled) {
			GenerateDynamicDebugMPMParticleInstances();
		}
	}

	void ParticleContext::SetMPMDebugNodeGenEnabled(bool enabled)
	{
		generate_mpm_node_instances_ = enabled;

		if (enabled) {
			GenerateDynamicDebugMPMNodeInstances();
		}
	}
#endif

	void ParticleContext::GenerateDynamicParticleMesh(const std::vector<MaterialPoint>& particles)
	{
#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}

		if (generate_mpm_node_instances_) {
			GenerateDynamicDebugMPMNodeInstances();
		}
#endif

		Mesh* mesh{ new Mesh{} };
		mesh->geometries.resize(1);

		// Generate vertices.
		uint32_t particle_vert_count{};
		{
			std::vector<Vertex> particle_vertices{ GetParticleVertices() };
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
		renderer_->ReplaceRenderObject(ro_target_, mesh, { 0 });
	}

#ifdef EDITOR_ENABLED
	void ParticleContext::GenerateDynamicDebugMPMParticleInstances()
	{
		const std::vector<MaterialPoint>& particles{ has_played_ ? mpm_context_.GetParticles() : StaticParticleToDynamic(static_particles_, side_flags_) };
		if (particles.empty()) {
			return;
		}

		std::vector<MPMDebugParticleInstance> mpm_particle_instances(particles.size());

		for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
		{
			mpm_particle_instances[p].mass = particles[p].mass;
			mpm_particle_instances[p].mu = particles[p].mu;
			mpm_particle_instances[p].lambda = particles[p].lambda;
			mpm_particle_instances[p].position = particles[p].position;
			mpm_particle_instances[p].velocity = particles[p].velocity;
			mpm_particle_instances[p].deformation_gradient_col_0 = particles[p].deformation_gradient[0];
			mpm_particle_instances[p].deformation_gradient_col_1 = particles[p].deformation_gradient[1];
			mpm_particle_instances[p].deformation_gradient_col_2 = particles[p].deformation_gradient[2];
		}

		renderer_->editor_backend_.SetMPMDebugParticleInstances(mpm_particle_instances);
	}

	void ParticleContext::GenerateDynamicDebugMPMNodeInstances()
	{
		const std::vector<GridNode>& nodes{ has_played_ ? mpm_context_.GetNodes() : std::vector<GridNode>(GRID_NODE_COUNT) };
		if (nodes.empty()) {
			return;
		}

		std::vector<MPMDebugNodeInstance> mpm_node_instances(nodes.size());

		for (uint32_t n{ 0 }; n < (uint32_t)nodes.size(); ++n)
		{
			mpm_node_instances[n].mass = nodes[n].mass;
			mpm_node_instances[n].position = nodes[n].position;
			mpm_node_instances[n].velocity = nodes[n].velocity;
			mpm_node_instances[n].momentum = nodes[n].momentum;
			mpm_node_instances[n].force = nodes[n].force;
		}

		renderer_->editor_backend_.SetMPMDebugNodeInstances(mpm_node_instances);
	}
#endif

	void ParticleContext::GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags)
	{
		has_played_ = false;

		// When true, forces to always generate dynamic particle meshes for debugging purposes.
		if (DISABLE_STATIC_PARTICLE_MESH)
		{
			GenerateDynamicParticleMesh(StaticParticleToDynamic(particles, side_flags));
			return;
		}

		// TODO: Experiment and measure speed with reinterpretting particles as uint64_t.
		//constexpr uint64_t empty_particles{ 0 };
		//constexpr uint64_t all_sides{ 0x3F3f3f3f3F3f3f3f };

		StaticParticleMeshGenerator gen{};
		Mesh* mesh{ gen.Generate(particles, side_flags) };
		renderer_->ReplaceRenderObject(ro_target_, mesh, { 0 });
	}

	std::vector<Vertex> ParticleContext::GetParticleVertices() const
	{
		uint32_t vert_count{ 24 }; // Cube with 3 normals per corner so 8 * 3 vertices.
		std::vector<Vertex> verts(vert_count);
		float particle_width{ chunk_width_ / CHUNK_ROW_VOXEL_COUNT };
		float particle_radius{ particle_width / 2.0f };

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

	std::vector<uint32_t> ParticleContext::GetParticleIndices() const
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

	std::vector<MaterialPoint> ParticleContext::StaticParticleToDynamic(const std::vector<StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags) const
	{
		if (static_particles.empty()) {
			return {};
		}

		float particle_width{ chunk_width_ / CHUNK_ROW_VOXEL_COUNT };
		std::vector<MaterialPoint> dynamic_particles{};
		for (uint32_t i{ 0 }; i < CHUNK_TOTAL_VOXEL_COUNT; ++i)
		{
			bool empty{ static_particles[i].type == ParticleType::EMPTY };
			bool occluded{ side_flags[i] == (uint8_t)ParticleSidesFlagBits::ALL_SIDES };

			if (empty || occluded) {
				continue;
			}

			MaterialPoint particle{
				.position = particle_width * glm::vec3(ParticleIndexToCoordinate(i)),
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	Mesh* StaticParticleMeshGenerator::Generate(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags)
	{
		mesh_ = new Mesh{};
		mesh_->geometries.emplace_back();

		GenerateSide(ParticleSidesFlagBits::X_POSITIVE, particles, side_flags);
		GenerateSide(ParticleSidesFlagBits::Y_POSITIVE, particles, side_flags);
		GenerateSide(ParticleSidesFlagBits::Z_POSITIVE, particles, side_flags);

		CalculateTangents(mesh_);
		return mesh_;
	}

	void StaticParticleMeshGenerator::GenerateSide(ParticleSidesFlagBits side, const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags)
	{
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
					bool solid_particle{ particles[i].type != ParticleType::EMPTY };
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
		case ParticleSidesFlagBits::X_NEGATIVE:
			top_left = { {     (float)depth, top,    left,  0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			top_right = { {    (float)depth, top,    right, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			bottom_right = { { (float)depth, bottom, right, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			bottom_left = { {  (float)depth, bottom, left,  0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };
			switch_winding_order = true;
			break;
		case ParticleSidesFlagBits::Y_POSITIVE:
			++depth;
		case ParticleSidesFlagBits::Y_NEGATIVE:
			top_left = { {     left,  (float)depth, top,    0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			top_right = { {    right, (float)depth, top,    0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			bottom_right = { { right, (float)depth, bottom, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			bottom_left = { {  left,  (float)depth, bottom, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f} };
			switch_winding_order = true;
			break;
		case ParticleSidesFlagBits::Z_POSITIVE:
			++depth;
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
