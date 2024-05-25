#pragma once

#include "particles.h"
#include "rigid_body.h"
#include "constraint.h"

namespace pmk
{
	struct PhysicsMaterial
	{
		uint32_t render_material;
		uint32_t jacobi_constraints_mask; // Each bit corresponds to an index of PhysicsContext::jacobi_constraints_.
		float density;                    // Kilograms per cubic meter.
		bool rigid_body;                  // True if it includes the rigid body constraint.
	};

	class PhysicsContext
	{
	public:
		void Initialize(Scene* scene, renderer::VulkanRenderer* renderer);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		// Returns list of node indices/IDs created (eg from rigid bodies).
		std::vector<uint32_t> EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void Reset();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		uint32_t GenerateVoxelsOnNode(Node* node);

		void TransferStaticParticlesToMPM();

		PhysicsMaterial* NewPhysicsMaterial();

		void DeletePhysicsMaterial(uint8_t physics_mat_index);

		std::vector<int> GetAllPhysicsMaterialRender();

		// Set the physics material's index into render materials. Determines how each physics material is rendered.
		void SetPhysicsMaterialRender(uint8_t physics_mat_index, uint32_t render_mat_index);

		// Get the physics material's index into render materials.
		uint32_t GetPhysicsMaterialRender(uint8_t physics_mat_index);

		void SetPhysicsMaterialConstraintMask(uint8_t physics_mat_index, uint32_t mask);

		uint32_t GetPhysicsMaterialConstraintMask(uint8_t physics_mat_index);

		PhysicsMaterial* GetPhysicsMaterial(uint8_t physics_mat_index);

		std::vector<std::pair<float*, std::string>> GetConstraintParameters(uint8_t constraint_index);

		void ConstraintParametersMutated(uint8_t constraint_index);

		// Write physics data to json.
		void DumpPhysicsMaterials(nlohmann::json& j);

		void LoadPhysicsMaterials(nlohmann::json& j);

#ifdef EDITOR_ENABLED
		void SetMPMDebugParticleGenEnabled(bool enabled);

		void SetRigidBodyOverlayEnabled(bool enabled);
#endif

	private:
		void UpdatePhysicsRenderMaterials();

		Scene* scene_{};
		renderer::VulkanRenderer* renderer_{};

		ParticleContext particle_context_{};
		RigidBodyContext rigid_body_context_{};

		std::vector<XPBDConstraint*> jacobi_constraints_{};
		std::vector<PhysicsMaterial*> physics_materials_{};
	};
}