#pragma once

#include "particles.h"
#include "rigid_body.h"

namespace pmk
{
	class PhysicsContext
	{
	public:
		void Initialize(Scene* scene, renderer::VulkanRenderer* renderer);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetParticles();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		uint32_t GenerateParticlesOnNode(Node* node);

		uint32_t GenerateTestParticleOnNode(Node* node);

		void TransferStaticParticlesToMPM();

		PhysicsMaterial* NewPhysicsMaterial();

		void DeletePhysicsMaterial(uint8_t physics_mat_index);

		std::vector<int> GetAllPhysicsMaterialRender();

		// Set the physics material's index into render materials. Determines how each physics material is rendered.
		void SetPhysicsMaterialRender(uint8_t physics_mat_index, uint32_t render_mat_index);

		// Get the physics material's index into render materials.
		uint32_t GetPhysicsMaterialRender(uint8_t physics_mat_index);

		template<typename T>
		void SetPhysicsMaterialModel(uint8_t physics_mat_index)
		{
			PhysicsMaterial* mat{ physics_materials_[physics_mat_index] };
			delete mat->constitutive_model;
			mat->constitutive_model = new T{};

			// Model only needs MPM context if it's an MPM model.
			MPMConstitutiveModel* mpm_model{ dynamic_cast<MPMConstitutiveModel*>(mat->constitutive_model) };
			if (mpm_model) {
				mpm_model->Initialize(particle_context_.GetMPMContext());
			}
		}

		ConstitutiveModel* GetPhysicsMaterialModel(uint8_t physics_mat_index);

		std::vector<std::pair<float*, std::string>> GetPhysicsParameters(uint8_t physics_mat_index);

		void PhysicsParametersMutated(uint8_t physics_mat_index);

#ifdef EDITOR_ENABLED
		void SetMPMDebugParticleGenEnabled(bool enabled);

		void SetMPMDebugNodeGenEnabled(bool enabled);
#endif

	private:
		void UpdatePhysicsRenderMaterials();

		Scene* scene_{};
		renderer::VulkanRenderer* renderer_{};

		ParticleContext particle_context_{};
		RigidBodyContext rigid_body_context_{};

		std::vector<PhysicsMaterial*> physics_materials_{};
	};
}