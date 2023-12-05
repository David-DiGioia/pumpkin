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

		mpm_context_.SimulateStep(delta_time);
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
		static_particles_ = renderer_->InvokeParticleGenShader(node->render_object);
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
				.constitutive_model_index = coord.y > 32 ? ConstitutiveModelIndex::HYPER_ELASTIC : ConstitutiveModelIndex::SNOW,
			};

			mpm_particles.push_back(mpm_particle);
		}

		mpm_context_.Initialize(std::move(mpm_particles), CHUNK_WIDTH);
	}

	void ParticleContext::GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, const std::vector<MaterialPoint>& particles) const
	{
		const std::byte* particle_buffer{ (const std::byte*)particles.data() };
		renderer_->GenerateDynamicParticleMesh(ro_target, particle_buffer, (uint32_t)particles.size(), offsetof(MaterialPoint, position), sizeof(MaterialPoint));
	}
}
