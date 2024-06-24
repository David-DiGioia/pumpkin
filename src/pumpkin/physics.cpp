#include "physics.h"

namespace jsonkey
{
	const std::string PHYSICS_MATERIALS{ "physics_materials" };
	// Material members.
	const std::string RENDER_MATERIAL_INDEX{ "render_material_index" };
	const std::string JACOBI_CONSTRAINTS_MASK{ "jacobi_constraints_mask" };
	const std::string DENSITY{ "density" };
	const std::string RIGID_BODY{ "rigid_body" };
	// End material members.

	const std::string CONSTRAINTS{ "constraints" };
	const std::string CONSTRAINT_TYPE{ "constraint_type" };
	// Constraint members.
	const std::string COMPLIANCE{ "compliance" };
	// Begin fluid density constraint.
	const std::string FLUID_DENSITY_CONSTRAINT{ "fluid_density" };
	const std::string FLUID_DENSITY_REST_DENSITY{ "rest_density" };
	// End fluid density constraint.
	// Begin collision constraint.
	const std::string COLLISION_CONSTRAINT{ "collision" };
	// End collision constraint.
	// Begin rigid body constraint.
	const std::string RIGID_BODY_CONSTRAINT{ "rigid_body" };
	// End rigid body constraint.
	// End constraint members.
}

namespace pmk
{
	void PhysicsContext::Initialize(Scene* scene, renderer::VulkanRenderer* renderer)
	{
		scene_ = scene;
		renderer_ = renderer;
		voxel_context_.Initialize(renderer, &jacobi_constraints_, &physics_materials_);
		rigid_body_context_.Initialize(renderer, scene, &physics_materials_);
	}

	void PhysicsContext::CleanUp()
	{
		voxel_context_.CleanUp();
		rigid_body_context_.CleanUp();
	}

	void PhysicsContext::PhysicsUpdate(float delta_time)
	{
		constexpr uint32_t substeps{ 3 };
		float h{ delta_time / substeps };

		for (uint32_t i{ 0 }; i < substeps; ++i)
		{
			rigid_body_context_.PhysicsUpdate(h);
			voxel_context_.PhysicsUpdate(h, &rigid_body_context_);
			rigid_body_context_.UpdateFromParticles(h, voxel_context_.GetXPBDContext());
		}

		voxel_context_.CopyPositionsToParticles();

#ifdef EDITOR_ENABLED
		if (rigid_body_context_.GetPhysicsUpdateEnabled()) {
			rigid_body_context_.GenerateDynamicDebugRbVoxelInstances();
		}
#endif
		if (voxel_context_.GetPhysicsUpdateEnabled()) {
			voxel_context_.GenerateDynamicMesh();
		}
	}

	std::vector<uint32_t> PhysicsContext::EnablePhysicsUpdate()
	{
		bool voxel_chunk_empty{};
		std::vector<uint32_t> node_ids{ rigid_body_context_.CreateRigidBodiesByConnectedness(voxel_context_.GetVoxelChunk(), &voxel_chunk_empty) };
		rigid_body_context_.EnablePhysicsUpdate();

		if (voxel_chunk_empty) {
			voxel_context_.DestroyVoxelRenderObject();
		}
		else {
			voxel_context_.EnablePhysicsUpdate();
		}
		UpdatePhysicsRenderMaterials();
		return node_ids;
	}

	void PhysicsContext::DisablePhysicsUpdate()
	{
		rigid_body_context_.DisablePhysicsUpdate();
		voxel_context_.DisablePhysicsUpdate();
	}

	void PhysicsContext::Reset()
	{
		rigid_body_context_.ResetRigidBodies();
		voxel_context_.ResetParticles();
	}

	bool PhysicsContext::GetPhysicsUpdateEnabled() const
	{
		return voxel_context_.GetPhysicsUpdateEnabled() || rigid_body_context_.GetPhysicsUpdateEnabled();
	}

	bool PhysicsContext::GetParticlesEmpty() const
	{
		return voxel_context_.GetParticlesEmpty();
	}

	uint32_t PhysicsContext::GenerateVoxelsOnNode(Node* node)
	{
		return voxel_context_.GenerateVoxelsOnNode(node);
	}

	void PhysicsContext::TransferStaticParticlesToMPM()
	{
		voxel_context_.TransferStaticParticlesToXPBD();
	}

	PhysicsMaterial* PhysicsContext::NewPhysicsMaterial()
	{
		PhysicsMaterial* new_material{ new PhysicsMaterial{} };
		new_material->jacobi_constraints_mask = 0x0;
		new_material->density = 1000.0f; // Density of water by default.
		physics_materials_.push_back(new_material);
		UpdatePhysicsRenderMaterials();
		return new_material;
	}

	void PhysicsContext::DeletePhysicsMaterial(uint8_t physics_mat_index)
	{
		physics_materials_.erase(physics_materials_.begin() + physics_mat_index);
		UpdatePhysicsRenderMaterials();
	}

	std::vector<int> PhysicsContext::GetAllPhysicsMaterialRender()
	{
		std::vector<int> result{};
		result.resize(physics_materials_.size());
		std::transform(physics_materials_.begin(), physics_materials_.end(), result.begin(),
			[](pmk::PhysicsMaterial* mat) { return mat->render_material; });
		return result;
	}

	// Set the physics material's index into render materials. Determines how each physics material is rendered.
	void PhysicsContext::SetPhysicsMaterialRender(uint8_t physics_mat_index, uint32_t render_mat_index)
	{
		physics_materials_[physics_mat_index]->render_material = render_mat_index;
		UpdatePhysicsRenderMaterials();
	}

	// Get the physics material's index into render materials.
	uint32_t PhysicsContext::GetPhysicsMaterialRender(uint8_t physics_mat_index)
	{
		return physics_materials_[physics_mat_index]->render_material;
	}

	void PhysicsContext::SetPhysicsMaterialConstraintMask(uint8_t physics_mat_index, uint32_t mask)
	{
		PhysicsMaterial* mat{ physics_materials_[physics_mat_index] };
		mat->jacobi_constraints_mask = mask;

		// If any of the constrains in the mask are rigid body constraints, mark the physics material to indicate that.
		mat->rigid_body = false;
		uint32_t i{ 0 };
		while (mask)
		{
			if ((mask & 1) && dynamic_cast<RigidBodyConstraint*>(jacobi_constraints_[i])) {
				mat->rigid_body = true;
			}
			mask >>= 1;
			++i;
		}
	}

	uint32_t PhysicsContext::GetPhysicsMaterialConstraintMask(uint8_t physics_mat_index)
	{
		return physics_materials_[physics_mat_index]->jacobi_constraints_mask;
	}

	PhysicsMaterial* PhysicsContext::GetPhysicsMaterial(uint8_t physics_mat_index)
	{
		return physics_materials_[physics_mat_index];
	}

	XPBDConstraint* PhysicsContext::NewConstraint()
	{
		FluidDensityConstraint* constraint{ new FluidDensityConstraint{} };
		jacobi_constraints_.push_back(constraint);
		return constraint;
	}

	void PhysicsContext::DeleteConstraint(uint32_t selected_idx)
	{
		jacobi_constraints_.erase(jacobi_constraints_.begin() + selected_idx);
	}

	pmk::XPBDConstraint* PhysicsContext::GetConstraint(uint32_t constraint_index)
	{
		return jacobi_constraints_[constraint_index];
	}

	std::vector<std::pair<float*, std::string>> PhysicsContext::GetConstraintParameters(uint8_t constraint_index)
	{
		return jacobi_constraints_[constraint_index]->GetParameters();
	}

	void PhysicsContext::ConstraintParametersMutated(uint8_t constraint_index)
	{
		jacobi_constraints_[constraint_index]->OnParametersMutated();
	}

	// TODO: Figure out why rigid body constraint doesn't save properly!

	void PhysicsContext::DumpPhysicsMaterials(nlohmann::json& j)
	{
		// Save physics materials.
		for (const PhysicsMaterial* material : physics_materials_)
		{
			nlohmann::json json_physics_mat{
				{ jsonkey::RENDER_MATERIAL_INDEX, material->render_material },
				{ jsonkey::JACOBI_CONSTRAINTS_MASK, material->jacobi_constraints_mask },
				{ jsonkey::DENSITY, material->density },
				{ jsonkey::RIGID_BODY, material->rigid_body },
			};

			j[jsonkey::PHYSICS_MATERIALS] += json_physics_mat;
		}

		// Save constraints.
		for (const XPBDConstraint* constraint : jacobi_constraints_)
		{
			const FluidDensityConstraint* fluid_density_constraint{ dynamic_cast<const FluidDensityConstraint*>(constraint) };
			if (fluid_density_constraint)
			{
				nlohmann::json json_constraint{
					{ jsonkey::CONSTRAINT_TYPE, jsonkey::FLUID_DENSITY_CONSTRAINT },
					{ jsonkey::FLUID_DENSITY_REST_DENSITY, fluid_density_constraint->rest_density_ },
				};

				j[jsonkey::CONSTRAINTS] += json_constraint;
				continue;
			}

			const CollisionConstraint* collision_constraint{ dynamic_cast<const CollisionConstraint*>(constraint) };
			if (collision_constraint)
			{
				nlohmann::json json_constraint{
					{ jsonkey::CONSTRAINT_TYPE, jsonkey::COLLISION_CONSTRAINT },
					{ jsonkey::COMPLIANCE, collision_constraint->compliance_ },
				};

				j[jsonkey::CONSTRAINTS] += json_constraint;
				continue;
			}

			const RigidBodyConstraint* rb_constraint{ dynamic_cast<const RigidBodyConstraint*>(constraint) };
			if (rb_constraint)
			{
				nlohmann::json json_constraint{
					{ jsonkey::CONSTRAINT_TYPE, jsonkey::RIGID_BODY_CONSTRAINT },
				};

				j[jsonkey::CONSTRAINTS] += json_constraint;
				continue;
			}
		}
	}

	void PhysicsContext::LoadPhysicsMaterials(nlohmann::json& j)
	{
		uint8_t physics_mat_idx{ 0 };
		for (auto& json_physics_mat : j[jsonkey::PHYSICS_MATERIALS])
		{
			PhysicsMaterial* physics_mat{ NewPhysicsMaterial() };
			physics_mat->render_material = json_physics_mat[jsonkey::RENDER_MATERIAL_INDEX];
			physics_mat->jacobi_constraints_mask = json_physics_mat[jsonkey::JACOBI_CONSTRAINTS_MASK];
			physics_mat->density = json_physics_mat[jsonkey::DENSITY];
			physics_mat->rigid_body = json_physics_mat[jsonkey::RIGID_BODY];
			++physics_mat_idx;
		}

		uint32_t constraint_idx{ 0 };
		for (auto& json_constraint : j[jsonkey::CONSTRAINTS])
		{
			NewConstraint();

			std::string constraint_type{ json_constraint[jsonkey::CONSTRAINT_TYPE] };
			if (constraint_type == jsonkey::FLUID_DENSITY_CONSTRAINT)
			{
				SetConstraintType<FluidDensityConstraint>(constraint_idx);
				FluidDensityConstraint* fluid_density_constraint{ (FluidDensityConstraint*)jacobi_constraints_.back() };
				fluid_density_constraint->rest_density_ = json_constraint[jsonkey::FLUID_DENSITY_REST_DENSITY];
			}
			else if (constraint_type == jsonkey::COLLISION_CONSTRAINT)
			{
				SetConstraintType<CollisionConstraint>(constraint_idx);
				CollisionConstraint* collision_constraint{ (CollisionConstraint*)jacobi_constraints_.back() };
				collision_constraint->compliance_ = json_constraint[jsonkey::COMPLIANCE];
			}
			else if (constraint_type == jsonkey::RIGID_BODY_CONSTRAINT) {
				SetConstraintType<RigidBodyConstraint>(constraint_idx);
			}
			++constraint_idx;
		}

		UpdatePhysicsRenderMaterials();
	}

#ifdef EDITOR_ENABLED
	void PhysicsContext::SetMPMDebugParticleGenEnabled(bool enabled)
	{
		voxel_context_.SetMPMDebugParticleGenEnabled(enabled);
	}

	void PhysicsContext::SetRigidBodyOverlayEnabled(bool enabled)
	{
		rigid_body_context_.SetRigidBodyOverlayEnabled(enabled);
	}
#endif

	void PhysicsContext::UpdatePhysicsRenderMaterials()
	{
		voxel_context_.UpdatePhysicsRenderMaterials(GetAllPhysicsMaterialRender());
	}
}