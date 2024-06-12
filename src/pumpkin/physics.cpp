#include "physics.h"

namespace jsonkey
{
	const std::string PHYSICS_MATERIALS{ "physics_materials" };
	// Material members.
	const std::string RENDER_MATERIAL_INDEX{ "render_material_index" };
	const std::string CONSTITUTIVE_MODEL_TYPE{ "constitutive_model_type" };
	const std::string CONSTITUTIVE_MODEL_DENSITY{ "density" };
	// Begin hyper elastic model.
	const std::string HYPER_ELASTIC_MODEL{ "hyper_elastic" };
	const std::string HYPER_ELASTIC_YOUNGS_MODULUS{ "youngs_modulus" };
	const std::string HYPER_ELASTIC_POISSONS_RATIO{ "poissons_ratio" };
	const std::string HYPER_ELASTIC_MU{ "mu" };
	const std::string HYPER_ELASTIC_LAMBDA{ "lambda" };
	// End hyper elastic model.
	// Begin fluid model.
	const std::string FLUID_MODEL{ "fluid" };
	const std::string FLUID_REST_DENSITY{ "rest_density" };
	const std::string FLUID_DYNAMIC_VISCOSITY{ "dynamic_viscosity" };
	// End fluid model.
	// Begin rigid body model.
	const std::string RIGID_BODY_MODEL{ "rigid_body" };
	// End rigid body model.
	// End material members.
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
		rigid_body_context_.PhysicsUpdate(delta_time);
		voxel_context_.PhysicsUpdate(delta_time, &rigid_body_context_);
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

	std::vector<std::pair<float*, std::string>> PhysicsContext::GetConstraintParameters(uint8_t constraint_index)
	{
		return jacobi_constraints_[constraint_index]->GetParameters();
	}

	void PhysicsContext::ConstraintParametersMutated(uint8_t constraint_index)
	{
		jacobi_constraints_[constraint_index]->OnParametersMutated();
	}

	void PhysicsContext::DumpPhysicsMaterials(nlohmann::json& j)
	{
		/*
		// Save physics materials.
		for (const PhysicsMaterial* material : physics_materials_)
		{
			ConstitutiveModel* model{ material->constitutive_model };

			nlohmann::json json_physics_mat{
				{ jsonkey::RENDER_MATERIAL_INDEX, material->render_material },
				{ jsonkey::CONSTITUTIVE_MODEL_DENSITY, model->GetDensity() },
			};

			HyperElasticModel* hyper_elastic_model{ dynamic_cast<HyperElasticModel*>(model) };
			if (hyper_elastic_model)
			{
				json_physics_mat[jsonkey::CONSTITUTIVE_MODEL_TYPE] = jsonkey::HYPER_ELASTIC_MODEL;
				json_physics_mat[jsonkey::HYPER_ELASTIC_YOUNGS_MODULUS] = hyper_elastic_model->youngs_modulus_;
				json_physics_mat[jsonkey::HYPER_ELASTIC_POISSONS_RATIO] = hyper_elastic_model->poissons_ratio_;
				json_physics_mat[jsonkey::HYPER_ELASTIC_MU] = hyper_elastic_model->mu_;
				json_physics_mat[jsonkey::HYPER_ELASTIC_LAMBDA] = hyper_elastic_model->lambda_;
			}

			FluidModel* fluid_model{ dynamic_cast<FluidModel*>(model) };
			if (fluid_model)
			{
				json_physics_mat[jsonkey::CONSTITUTIVE_MODEL_TYPE] = jsonkey::FLUID_MODEL;
				json_physics_mat[jsonkey::FLUID_REST_DENSITY] = fluid_model->rest_density_;
				json_physics_mat[jsonkey::FLUID_DYNAMIC_VISCOSITY] = fluid_model->dynamic_viscosity_;
			}

			RigidBodyConstraint* rigid_body_model{ dynamic_cast<RigidBodyConstraint*>(model) };
			if (rigid_body_model)
			{
				json_physics_mat[jsonkey::CONSTITUTIVE_MODEL_TYPE] = jsonkey::RIGID_BODY_MODEL;
			}

			j[jsonkey::PHYSICS_MATERIALS] += json_physics_mat;
		}
		*/
	}

	void PhysicsContext::LoadPhysicsMaterials(nlohmann::json& j)
	{
		/*
		uint8_t physics_mat_idx{ 0 };
		for (auto& json_physics_mat : j[jsonkey::PHYSICS_MATERIALS])
		{
			PhysicsMaterial* physics_mat{ NewPhysicsMaterial() };
			physics_mat->render_material = json_physics_mat[jsonkey::RENDER_MATERIAL_INDEX];
			physics_mat->constitutive_model->density_ = json_physics_mat[jsonkey::CONSTITUTIVE_MODEL_DENSITY];

			std::string model_type{ json_physics_mat[jsonkey::CONSTITUTIVE_MODEL_TYPE] };

			if (model_type == jsonkey::HYPER_ELASTIC_MODEL)
			{
				SetPhysicsMaterialModel<HyperElasticModel>(physics_mat_idx);
				HyperElasticModel* model{ (HyperElasticModel*)physics_mat->constitutive_model };
				model->youngs_modulus_ = json_physics_mat[jsonkey::HYPER_ELASTIC_YOUNGS_MODULUS];
				model->poissons_ratio_ = json_physics_mat[jsonkey::HYPER_ELASTIC_POISSONS_RATIO];
				model->mu_ = json_physics_mat[jsonkey::HYPER_ELASTIC_MU];
				model->lambda_ = json_physics_mat[jsonkey::HYPER_ELASTIC_LAMBDA];
			}
			else if (model_type == jsonkey::FLUID_MODEL)
			{
				SetPhysicsMaterialModel<FluidModel>(physics_mat_idx);
				FluidModel* model{ (FluidModel*)physics_mat->constitutive_model };
				model->rest_density_ = json_physics_mat[jsonkey::FLUID_REST_DENSITY];
				model->dynamic_viscosity_ = json_physics_mat[jsonkey::FLUID_DYNAMIC_VISCOSITY];
			}
			else if (model_type == jsonkey::RIGID_BODY_MODEL)
			{
				SetPhysicsMaterialModel<RigidBodyConstraint>(physics_mat_idx);
			}
			++physics_mat_idx;
		}
		UpdatePhysicsRenderMaterials();
		*/
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