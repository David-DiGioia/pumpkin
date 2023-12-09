#include "particles.h"

#include "tracy/Tracy.hpp"
#include "vulkan_renderer.h"
#include "common_constants.h"
#include "scene.h"

namespace pmk
{
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

	void ParticleContext::Initialize(renderer::VulkanRenderer* renderer)
	{
		renderer_ = renderer;
	}

	void ParticleContext::CleanUp()
	{
	}

	void ParticleContext::PhysicsUpdate(float delta_time)
	{
		if (!update_physics_) {
			return;
		}

		constexpr uint32_t sub_steps{ 3 };
		for (uint32_t i{ 0 }; i < sub_steps; ++i) {
			mpm_context_.SimulateStep(delta_time / sub_steps);
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
		TransferStaticParticlesToMPM();
		if (!update_physics_) {
			GenerateDynamicParticleMesh(particle_node_->render_object, mpm_context_.GetParticles());
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

	uint32_t ParticleContext::GenerateParticlesOnNode(Node* node)
	{
		particle_node_ = node;
		renderer_->InvokeParticleGenShader(node->render_object, &static_particles_, &side_flags_);
		GenerateStaticParticleMesh(node->render_object);
		renderer_->UpdateMaterials();

		uint32_t particle_count{};
		for (renderer::StaticParticle p : static_particles_)
		{
			if (p.type != renderer::ParticleTypeIndex::EMPTY) {
				++particle_count;
			}
		}
		return particle_count;
	}

	uint32_t ParticleContext::GenerateTestParticleOnNode(Node* node)
	{
		particle_node_ = node;
		constexpr float youngs_modulus{ 10.0f };
		constexpr float poissons_ratio{ 0.4f };
		constexpr float mu{ CalculateMu(youngs_modulus, poissons_ratio) };
		constexpr float lambda{ CalculateLambda(youngs_modulus, poissons_ratio) };
		MaterialPoint mpm_particle{
			.mass = .01f,
			.mu = mu,
			.lambda = lambda,
			.position = glm::vec3{0.321932f, 0.452119f, 0.434341f},
			.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
			.affine_matrix = glm::mat3{0.0f},
			.deformation_gradient_elastic = glm::mat3{1.0f},
			.deformation_gradient_plastic = glm::mat3{1.0f},
		};
		std::vector<MaterialPoint> mpm_particles{ mpm_particle };
		mpm_context_.Initialize(std::move(mpm_particles), CHUNK_WIDTH);
		// Since we don't use static particles here, we just simulate a single set to get all the MPM
		// particle info set that needs to be set.
		update_physics_ = true;
		has_played_ = true;
		PhysicsUpdate(1.0f / 60.0f);
		update_physics_ = false;

		GenerateDynamicParticleMesh(node->render_object, mpm_particles);
		return 1;
	}

	void ParticleContext::TransferStaticParticlesToMPM()
	{
		std::vector<MaterialPoint> mpm_particles{};

		for (uint32_t i{ 0 }; i < (uint32_t)static_particles_.size(); ++i)
		{
			if (static_particles_[i].type == renderer::ParticleTypeIndex::EMPTY) {
				continue;
			}

			glm::uvec3 coord{ ParticleIndexToCoordinate(i) };

			MaterialPoint mpm_particle{
				.mass = {},   // Set later.
				.mu = {},     // Set later.
				.lambda = {}, // Set later.
				.position = PARTICLE_WIDTH * glm::vec3{coord},
				.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
				.affine_matrix = glm::mat3{0.0f},
				.deformation_gradient_elastic = glm::mat3{1.0f},
				.deformation_gradient_plastic = glm::mat3{1.0f},
				.constitutive_model_index = ConstitutiveModelIndex::HYPER_ELASTIC,
			};

			mpm_particles.push_back(mpm_particle);
		}

		mpm_context_.Initialize(std::move(mpm_particles), CHUNK_WIDTH);
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

	void ParticleContext::GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, const std::vector<MaterialPoint>& particles) const
	{
		const std::byte* particle_buffer{ (const std::byte*)particles.data() };
		renderer_->GenerateDynamicParticleMesh(ro_target, particle_buffer, (uint32_t)particles.size(), offsetof(MaterialPoint, position), sizeof(MaterialPoint));

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
		renderer_->GenerateStaticParticleMesh(ro_target, static_particles_, side_flags_);

#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}

		if (generate_mpm_node_instances_) {
			GenerateDynamicDebugMPMNodeInstances();
		}
#endif
	}

	std::vector<MaterialPoint> ParticleContext::StaticParticlesToMaterialPoints(const std::vector<renderer::StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags) const
	{
		if (static_particles.empty()) {
			return {};
		}

		std::vector<MaterialPoint> dynamic_particles{};
		for (uint32_t i{ 0 }; i < CHUNK_TOTAL_VOXEL_COUNT; ++i)
		{
			bool empty{ static_particles[i].type == renderer::ParticleTypeIndex::EMPTY };
			bool occluded{ side_flags[i] == (uint8_t)renderer::ParticleSidesFlagBits::ALL_SIDES };

			if (empty || occluded) {
				continue;
			}

			MaterialPoint particle{
				.position = PARTICLE_WIDTH * glm::vec3(ParticleIndexToCoordinate(i)),
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	void ParticleContext::GenerateDynamicDebugMPMParticleInstances() const
	{
		const std::vector<MaterialPoint>& particles{ has_played_ ? mpm_context_.GetParticles() : StaticParticlesToMaterialPoints(static_particles_, side_flags_) };
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
			mpm_node_instances[n].position = nodes[n].position;
			mpm_node_instances[n].velocity = nodes[n].velocity;
			mpm_node_instances[n].momentum = nodes[n].momentum;
			mpm_node_instances[n].force = nodes[n].force;
		}

		renderer_->SetMPMDebugNodeInstances(mpm_node_instances);
	}
}
