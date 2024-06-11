#include "voxels.h"

#include "tracy/Tracy.hpp"
#include "vulkan_renderer.h"
#include "common_constants.h"
#include "scene.h"

namespace pmk
{
	void VoxelContext::Initialize(
		renderer::VulkanRenderer* renderer,
		const std::vector<XPBDConstraint*>* jacobi_constraints,
		const std::vector<PhysicsMaterial*>* physics_materials)
	{
		renderer_ = renderer;
		physics_materials_ = physics_materials;
		jacobi_constraints_ = jacobi_constraints;
	}

	void VoxelContext::CleanUp()
	{
	}

	void VoxelContext::PhysicsUpdate(float delta_time, const std::vector<RigidBody*>& rigid_bodies)
	{
		if (!update_physics_) {
			return;
		}

		constexpr uint32_t sub_steps{ 1 };
		for (uint32_t i{ 0 }; i < sub_steps; ++i) {
			xpbd_context_.SimulateStep(delta_time / sub_steps, rigid_bodies);
		}
		GenerateDynamicParticleMesh(particle_node_->render_object, xpbd_context_.GetParticles());
	}

	void VoxelContext::EnablePhysicsUpdate()
	{
		if (!has_played_)
		{
			has_played_ = true;
			TransferStaticParticlesToXPBD();
		}
		update_physics_ = true;
	}

	void VoxelContext::DisablePhysicsUpdate()
	{
		update_physics_ = false;
	}

	void VoxelContext::ResetParticles()
	{
		has_played_ = false;
		DisablePhysicsUpdate();
		GenerateStaticParticleMesh(particle_node_->render_object);
	}

	bool VoxelContext::GetPhysicsUpdateEnabled() const
	{
		return update_physics_;
	}

	bool VoxelContext::GetParticlesEmpty() const
	{
		return (generated_voxel_count_ == 0) && xpbd_context_.GetParticles().empty();
	}

	uint32_t VoxelContext::GenerateVoxelsOnNode(Node* node)
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

	void VoxelContext::TransferStaticParticlesToXPBD()
	{
		std::vector<XPBDParticle> xpbd_particles{};

		for (uint32_t i{ 0 }; i < voxel_chunk_.VoxelCount(); ++i)
		{
			if (voxel_chunk_.IsEmpty(i)) {
				continue;
			}

			glm::uvec3 coord{ voxel_chunk_.IndexToCoordinate(i) };

			XPBDParticle xpbd_particle{
				.position = PARTICLE_WIDTH * glm::vec3{coord},
				.predicted_position = {},
				.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
				.inverse_mass = {},   // Set later.
				.physics_material_index = voxel_chunk_.Index(i).physics_material_index,
			};

			xpbd_particles.push_back(xpbd_particle);
		}

		xpbd_context_.Initialize(std::move(xpbd_particles), CHUNK_WIDTH, jacobi_constraints_, physics_materials_);
	}

	XPBDParticleContext* VoxelContext::GetXPBDContext()
	{
		return &xpbd_context_;
	}

	renderer::VoxelChunk& VoxelContext::GetVoxelChunk()
	{
		return voxel_chunk_;
	}

	void VoxelContext::UpdatePhysicsRenderMaterials(std::vector<int>&& all_physics_render_materials)
	{
		renderer_->SetPhysicsToRenderMaterialMap(std::move(all_physics_render_materials));
		if (particle_node_ && particle_node_->render_object != renderer::NULL_HANDLE) {
			renderer_->UpdatePhysicsRenderMaterials(particle_node_->render_object);
		}
	}

	void VoxelContext::DestroyVoxelRenderObject()
	{
		if (particle_node_)
		{
			renderer_->QueueDestroyRenderObject(particle_node_->render_object);
			particle_node_->render_object = renderer::NULL_HANDLE;
		}
	}

#ifdef EDITOR_ENABLED
	void VoxelContext::SetMPMDebugParticleGenEnabled(bool enabled)
	{
		generate_mpm_particle_instances_ = enabled;

		if (enabled) {
			GenerateDynamicDebugMPMParticleInstances();
		}
	}
#endif

	void VoxelContext::GenerateDynamicParticleMesh(renderer::RenderObjectHandle ro_target, std::vector<XPBDParticle>& particles) const
	{
		if (particles.empty()) {
			return;
		}

		{
			ZoneScopedN("Sort");
			// For some reason I will never know, sorting is actually faster than grouping here.
			std::sort(particles.begin(), particles.end(),
				[](const XPBDParticle& p0, const XPBDParticle& p1) { return p0.physics_material_index < p1.physics_material_index; });
		}

		std::vector<renderer::MaterialRange> mat_ranges{};
		{
			ZoneScopedN("Create material ranges");
			mat_ranges = renderer_->CreateMaterialRanges<XPBDParticle>(particles);
		}

		{
			ZoneScopedN("Generate mesh");
			renderer_->CmdGenerateDynamicParticleMesh(ro_target, (const std::byte*)particles.data(), (uint32_t)particles.size(), offsetof(XPBDParticle, position), sizeof(XPBDParticle), mat_ranges);
		}

#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}
#endif
	}

	void VoxelContext::GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target)
	{
		renderer_->GenerateStaticParticleMesh(ro_target, voxel_chunk_);

#ifdef EDITOR_ENABLED
		if (generate_mpm_particle_instances_) {
			GenerateDynamicDebugMPMParticleInstances();
		}
#endif
	}

	std::vector<XPBDParticle> VoxelContext::VoxelsToMaterialPoints(const renderer::VoxelChunk& voxel_chunk) const
	{
		if (voxel_chunk.VoxelCount() == 0) {
			return {};
		}

		std::vector<XPBDParticle> dynamic_particles{};
		for (uint32_t i{ 0 }; i < CHUNK_TOTAL_VOXEL_COUNT; ++i)
		{

			if (voxel_chunk.IsEmpty(i) || voxel_chunk.IsOccluded(i)) {
				continue;
			}

			XPBDParticle particle{
				.position = PARTICLE_WIDTH * glm::vec3(voxel_chunk_.IndexToCoordinate(i)),
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	void VoxelContext::GenerateDynamicDebugMPMParticleInstances() const
	{
		const std::vector<XPBDParticle>& particles{ has_played_ ? xpbd_context_.GetParticles() : VoxelsToMaterialPoints(voxel_chunk_) };
		if (particles.empty()) {
			return;
		}

		std::vector<renderer::XPBDDebugParticleInstance> xpbd_particle_instances(particles.size());

		for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
		{
			xpbd_particle_instances[p].position = particles[p].position;
			xpbd_particle_instances[p].predicted_position = particles[p].predicted_position;
			xpbd_particle_instances[p].velocity = particles[p].velocity;
			xpbd_particle_instances[p].inverse_mass = particles[p].inverse_mass;
		}

		renderer_->SetXPBDDebugParticleInstances(xpbd_particle_instances);
	}
}
