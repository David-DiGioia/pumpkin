#include "physics.h"

namespace pmk
{
	void PhysicsContext::Initialize(Scene* scene, renderer::VulkanRenderer* renderer)
	{
		scene_ = scene;
		renderer_ = renderer;
		particle_context_.Initialize(renderer, &physics_materials_);
		rigid_body_context_.Initialize(renderer, scene, &physics_materials_);
	}

	void PhysicsContext::CleanUp()
	{
		particle_context_.CleanUp();
		rigid_body_context_.CleanUp();
	}

	void PhysicsContext::PhysicsUpdate(float delta_time)
	{
		particle_context_.PhysicsUpdate(delta_time);
	}

	void PhysicsContext::EnablePhysicsUpdate()
	{
		rigid_body_context_.CreateRigidBodiesByConnectedness(particle_context_.GetVoxelChunk());
		particle_context_.EnablePhysicsUpdate();
	}

	void PhysicsContext::DisablePhysicsUpdate()
	{
		particle_context_.DisablePhysicsUpdate();
	}

	void PhysicsContext::ResetParticles()
	{
		particle_context_.ResetParticles();
	}

	bool PhysicsContext::GetPhysicsUpdateEnabled() const
	{
		return particle_context_.GetPhysicsUpdateEnabled();
	}

	bool PhysicsContext::GetParticlesEmpty() const
	{
		return particle_context_.GetParticlesEmpty();
	}

	uint32_t PhysicsContext::GenerateVoxelsOnNode(Node* node)
	{
		return particle_context_.GenerateVoxelsOnNode(node);
	}

	uint32_t PhysicsContext::GenerateTestParticleOnNode(Node* node)
	{
		return particle_context_.GenerateTestParticleOnNode(node);
	}

	void PhysicsContext::TransferStaticParticlesToMPM()
	{
		particle_context_.TransferStaticParticlesToMPM();
	}

	PhysicsMaterial* PhysicsContext::NewPhysicsMaterial()
	{
		PhysicsMaterial* new_material{ new PhysicsMaterial{} };
		FluidModel* fluid_model = new FluidModel{};
		fluid_model->Initialize(particle_context_.GetMPMContext());
		new_material->constitutive_model = fluid_model;
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

	ConstitutiveModel* PhysicsContext::GetPhysicsMaterialModel(uint8_t physics_mat_index)
	{
		return physics_materials_[physics_mat_index]->constitutive_model;
	}

	std::vector<std::pair<float*, std::string>> PhysicsContext::GetPhysicsParameters(uint8_t physics_mat_index)
	{
		return physics_materials_[physics_mat_index]->constitutive_model->GetParameters();
	}

	void PhysicsContext::PhysicsParametersMutated(uint8_t physics_mat_index)
	{
		physics_materials_[physics_mat_index]->constitutive_model->OnParametersMutated();
	}

#ifdef EDITOR_ENABLED
	void PhysicsContext::SetMPMDebugParticleGenEnabled(bool enabled)
	{
		particle_context_.SetMPMDebugParticleGenEnabled(enabled);
	}

	void PhysicsContext::SetMPMDebugNodeGenEnabled(bool enabled)
	{
		particle_context_.SetMPMDebugNodeGenEnabled(enabled);
	}
#endif

	void PhysicsContext::UpdatePhysicsRenderMaterials()
	{
		particle_context_.UpdatePhysicsRenderMaterials(GetAllPhysicsMaterialRender());
	}
}