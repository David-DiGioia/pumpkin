#include "particles.h"

#include "tracy/Tracy.hpp"
#include "vulkan_renderer.h"
#include "common_constants.h"
#include "scene.h"

namespace pmk
{
	void ParticleContext::Initialize(renderer::VulkanRenderer* renderer, const std::vector<PhysicsMaterial*>* physics_materials)
	{
		renderer_ = renderer;
		physics_materials_ = physics_materials;
	}

	void ParticleContext::CleanUp()
	{
	}

	void ParticleContext::PhysicsUpdate(float delta_time, const std::vector<RigidBody*>& rigid_bodies)
	{
		if (!update_physics_) {
			return;
		}

		constexpr uint32_t sub_steps{ 2 };
		for (uint32_t i{ 0 }; i < sub_steps; ++i) {
			mpm_context_.SimulateStep(delta_time / sub_steps, rigid_bodies);
		}
		GenerateDynamicParticleMesh(particle_node_->render_object, mpm_context_.GetParticles());
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
		GenerateStaticParticleMesh(particle_node_->render_object);
	}

	bool ParticleContext::GetPhysicsUpdateEnabled() const
	{
		return update_physics_;
	}

	bool ParticleContext::GetParticlesEmpty() const
	{
		return (generated_voxel_count_ == 0) && mpm_context_.GetParticles().empty();
	}

	uint32_t ParticleContext::GenerateVoxelsOnNode(Node* node)
	{
		if (renderer_->GetMaterials().empty()) {
			renderer_->CreateDefaultMaterial();
		}

		particle_node_ = node;
		renderer_->InvokeParticleGenShader(node->render_object, &voxel_chunk_.GetVoxels(), &voxel_chunk_.GetSideFlags());
		ResetParticles();
		renderer_->UpdateMaterials();

		generated_voxel_count_ = 0;
		for (uint32_t i{ 0 }; i < voxel_chunk_.VoxelCount(); ++i)
		{
			if (!voxel_chunk_.IsEmpty(i)) {
				++generated_voxel_count_;
			}
		}
		return generated_voxel_count_;
	}

	uint32_t ParticleContext::GenerateTestParticleOnNode(Node* node)
	{
		if (renderer_->GetMaterials().empty()) {
			renderer_->CreateDefaultMaterial();
		}

		particle_node_ = node;
		constexpr float youngs_modulus{ 10.0f };
		constexpr float poissons_ratio{ 0.4f };
		constexpr float mu{ CalculateMu(youngs_modulus, poissons_ratio) };
		constexpr float lambda{ CalculateLambda(youngs_modulus, poissons_ratio) };
		MaterialPoint mpm_particle{
			.mass = 1.0f,
			.mu = mu,
			.lambda = lambda,
			.position = glm::vec3{0.321932f, 0.452119f, 0.434341f},
			.velocity = glm::vec3{0.0f, 5.0f, 0.0f},
			.affine_matrix = glm::mat3{0.0f},
			.deformation_gradient_elastic = glm::mat3{1.0f},
			.deformation_gradient_plastic = glm::mat3{1.0f},
		};
		std::vector<MaterialPoint> mpm_particles{ mpm_particle };
		mpm_context_.Initialize(std::move(mpm_particles), CHUNK_WIDTH, physics_materials_);
		// Since we don't use static particles here, we just simulate a single set to get all the MPM
		// particle info set that needs to be set.
		update_physics_ = true;
		has_played_ = true;
		PhysicsUpdate(1.0f / 60.0f, {});
		update_physics_ = false;

		GenerateDynamicParticleMesh(node->render_object, mpm_context_.GetParticles());
		renderer_->UpdateMaterials();

		return 1;
	}

	void ParticleContext::TransferStaticParticlesToMPM()
	{
		std::vector<MaterialPoint> mpm_particles{};

		for (uint32_t i{ 0 }; i < voxel_chunk_.VoxelCount(); ++i)
		{
			if (voxel_chunk_.IsEmpty(i)) {
				continue;
			}

			glm::uvec3 coord{ voxel_chunk_.IndexToCoordinate(i) };

			MaterialPoint mpm_particle{
				.mass = {},   // Set later.
				.mu = {},     // Set later.
				.lambda = {}, // Set later.
				.position = PARTICLE_WIDTH * glm::vec3{coord},
				.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
				.affine_matrix = glm::mat3{0.0f},
				.deformation_gradient_elastic = glm::mat3{1.0f},
				.deformation_gradient_plastic = glm::mat3{1.0f},
				.physics_material_index = voxel_chunk_.Index(i).physics_material_index,
			};

			mpm_particles.push_back(mpm_particle);
		}

		mpm_context_.Initialize(std::move(mpm_particles), CHUNK_WIDTH, physics_materials_);
	}

	MPMContext* ParticleContext::GetMPMContext()
	{
		return &mpm_context_;
	}

	renderer::VoxelChunk& ParticleContext::GetVoxelChunk()
	{
		return voxel_chunk_;
	}

	void ParticleContext::UpdatePhysicsRenderMaterials(std::vector<int>&& all_physics_render_materials)
	{
		renderer_->SetPhysicsToRenderMaterialMap(std::move(all_physics_render_materials));
		if (particle_node_ && particle_node_->render_object != renderer::NULL_HANDLE) {
			renderer_->UpdatePhysicsRenderMaterials(particle_node_->render_object);
		}
	}

	void ParticleContext::DestroyVoxelRenderObject()
	{
		if (particle_node_)
		{
			renderer_->QueueDestroyRenderObject(particle_node_->render_object);
			particle_node_->render_object = renderer::NULL_HANDLE;
		}
	}

#ifdef EDITOR_ENABLED
	void ParticleContext::SetMPMDebugParticleGenEnabled(bool enabled)
	{
		generate_mpm_particle_instances_ = enabled;

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

	void ParticleContext::GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, std::vector<MaterialPoint>& particles) const
	{
		if (particles.empty()) {
			return;
		}

		{
			ZoneScopedN("Sort");
			// For some reason I will never know, sorting is actually faster than grouping here.
			std::sort(particles.begin(), particles.end(),
				[](const MaterialPoint& p0, const MaterialPoint& p1) { return p0.physics_material_index < p1.physics_material_index; });
		}

		std::vector<renderer::MaterialRange> mat_ranges{};
		{
			ZoneScopedN("Create material ranges");
			mat_ranges = renderer_->CreateMaterialRanges<MaterialPoint>(particles);
		}

		{
			ZoneScopedN("Generate mesh");
			renderer_->CmdGenerateDynamicParticleMesh(ro_target, (const std::byte*)particles.data(), (uint32_t)particles.size(), offsetof(MaterialPoint, position), sizeof(MaterialPoint), mat_ranges);
		}

#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}

		if (generate_mpm_node_instances_) {
			GenerateDynamicDebugMPMNodeInstances();
		}
#endif
	}

	void ParticleContext::GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target)
	{
		renderer_->GenerateStaticParticleMesh(ro_target, voxel_chunk_);

#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}

		if (generate_mpm_node_instances_) {
			GenerateDynamicDebugMPMNodeInstances();
		}
#endif
	}

	std::vector<MaterialPoint> ParticleContext::VoxelsToMaterialPoints(const renderer::VoxelChunk& voxel_chunk) const
	{
		if (voxel_chunk.VoxelCount() == 0) {
			return {};
		}

		std::vector<MaterialPoint> dynamic_particles{};
		for (uint32_t i{ 0 }; i < CHUNK_TOTAL_VOXEL_COUNT; ++i)
		{

			if (voxel_chunk.IsEmpty(i) || voxel_chunk.IsOccluded(i)) {
				continue;
			}

			MaterialPoint particle{
				.position = PARTICLE_WIDTH * glm::vec3(voxel_chunk_.IndexToCoordinate(i)),
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	void ParticleContext::GenerateDynamicDebugMPMParticleInstances() const
	{
		const std::vector<MaterialPoint>& particles{ has_played_ ? mpm_context_.GetParticles() : VoxelsToMaterialPoints(voxel_chunk_) };
		if (particles.empty()) {
			return;
		}

		std::vector<renderer::MPMDebugParticleInstance> mpm_particle_instances(particles.size());

		for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
		{
			mpm_particle_instances[p].mass = particles[p].mass;
			mpm_particle_instances[p].mu = particles[p].mu;
			mpm_particle_instances[p].lambda = particles[p].lambda;
			mpm_particle_instances[p].position = particles[p].position;
			mpm_particle_instances[p].velocity = particles[p].velocity;
			mpm_particle_instances[p].gradient = particles[p].gradient;
			mpm_particle_instances[p].elastic_col_0 = particles[p].deformation_gradient_elastic[0];
			mpm_particle_instances[p].elastic_col_1 = particles[p].deformation_gradient_elastic[1];
			mpm_particle_instances[p].elastic_col_2 = particles[p].deformation_gradient_elastic[2];
			mpm_particle_instances[p].plastic_col_0 = particles[p].deformation_gradient_plastic[0];
			mpm_particle_instances[p].plastic_col_1 = particles[p].deformation_gradient_plastic[1];
			mpm_particle_instances[p].plastic_col_2 = particles[p].deformation_gradient_plastic[2];
		}

		renderer_->SetMPMDebugParticleInstances(mpm_particle_instances);
	}

	void ParticleContext::GenerateDynamicDebugMPMNodeInstances() const
	{
		const std::vector<GridNode>& nodes{ has_played_ ? mpm_context_.GetNodes() : std::vector<GridNode>(GRID_NODE_COUNT) };
		if (nodes.empty()) {
			return;
		}

		std::vector<renderer::MPMDebugNodeInstance> mpm_node_instances(nodes.size());
		for (uint32_t n{ 0 }; n < (uint32_t)nodes.size(); ++n)
		{
			mpm_node_instances[n].mass = nodes[n].mass;
			mpm_node_instances[n].rigid_body_distance = nodes[n].rb_distance;
			mpm_node_instances[n].position = nodes[n].position;
			mpm_node_instances[n].velocity = nodes[n].velocity;
			mpm_node_instances[n].momentum = nodes[n].momentum;
			mpm_node_instances[n].force = nodes[n].force;

			// Make positive and negative tag visually distinct in debug render.
			if (GetTag(nodes[n], nodes[n].closest_rb_index) < 0.0f) {
				mpm_node_instances[n].rigid_body_distance *= 0.01f;
			}
		}

		renderer_->SetMPMDebugNodeInstances(mpm_node_instances);
	}
}
