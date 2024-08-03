#pragma once

#include "voxels.h"
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

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void Reset();

		bool GetPhysicsUpdateEnabled() const;

		void GenerateVoxels();

		Node* GetXPBDNode();

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

		XPBDConstraint* NewConstraint();

		void DeleteConstraint(uint32_t selected_idx);

		pmk::XPBDConstraint* GetConstraint(uint32_t constraint_index);

		template<typename T>
		pmk::XPBDConstraint* SetConstraintType(uint8_t constraint_index)
		{
			delete jacobi_constraints_[constraint_index];
			pmk::XPBDConstraint* new_constraint{ new T{} };
			jacobi_constraints_[constraint_index] = new_constraint;
			return new_constraint;
		}

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

		VoxelContext voxel_context_{};
		XPBDRigidBodyContext rigid_body_context_{};

		std::vector<XPBDConstraint*> jacobi_constraints_{};
		std::vector<PhysicsMaterial*> physics_materials_{};
	};
}