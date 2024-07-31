#include "voxels.h"

#include "tracy/Tracy.hpp"
#include "vulkan_renderer.h"
#include "common_constants.h"
#include "scene.h"

namespace pmk
{
	void VoxelContext::Initialize(
		Node* xpbd_node,
		renderer::VulkanRenderer* renderer,
		const std::vector<XPBDConstraint*>* jacobi_constraints,
		const std::vector<PhysicsMaterial*>* physics_materials)
	{
		xpbd_node_ = xpbd_node;
		renderer_ = renderer;
		jacobi_constraints_ = jacobi_constraints;
		physics_materials_ = physics_materials;

		xpbd_context_.Initialize(CHUNK_WIDTH, jacobi_constraints_, physics_materials_);
	}

	void VoxelContext::CleanUp()
	{
	}

	void VoxelContext::PhysicsUpdate(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		if (!update_physics_) {
			return;
		}

		xpbd_context_.SimulateStep(delta_time, rb_context);
	}

	void VoxelContext::EnablePhysicsUpdate()
	{
		has_played_ = true;
		update_physics_ = true;
	}

	void VoxelContext::DisablePhysicsUpdate()
	{
		update_physics_ = false;
	}

	bool VoxelContext::GetPhysicsUpdateEnabled() const
	{
		return update_physics_;
	}

	bool VoxelContext::GetParticlesEmpty() const
	{
		return (generated_voxel_count_ == 0) && xpbd_context_.GetParticles().empty();
	}

	void VoxelContext::GenerateVoxels()
	{
		if (renderer_->GetMaterials().empty()) {
			renderer_->CreateDefaultMaterial();
		}

		terrain_->GenerateVoxels();
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

	void VoxelContext::GenerateDynamicMesh()
	{
		GenerateDynamicParticleMesh(xpbd_node_->render_object, xpbd_context_.GetParticles());
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
			renderer_->CmdGenerateDynamicParticleMesh(ro_target, (const std::byte*)particles.data(), (uint32_t)particles.size(), offsetof(XPBDParticle, s.position), sizeof(XPBDParticle), mat_ranges);
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

	std::vector<XPBDParticle> VoxelContext::VoxelsToParticles(const renderer::VoxelChunk& voxel_chunk) const
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
				.s = {
					.position = PARTICLE_WIDTH * glm::vec3(voxel_chunk_.IndexToCoordinate(i)),
				},
			};
			dynamic_particles.push_back(particle);
		}
		return dynamic_particles;
	}

	void VoxelContext::GenerateDynamicDebugMPMParticleInstances() const
	{
		const std::vector<XPBDParticle>& particles{ has_played_ ? xpbd_context_.GetParticles() : VoxelsToParticles(voxel_chunk_) };
		if (particles.empty()) {
			return;
		}

		std::vector<renderer::XPBDDebugParticleInstance> xpbd_particle_instances(particles.size());

		for (uint32_t p{ 0 }; p < (uint32_t)particles.size(); ++p)
		{
			xpbd_particle_instances[p].position = particles[p].s.predicted_position;
			xpbd_particle_instances[p].velocity = particles[p].velocity;
			xpbd_particle_instances[p].debug_color = particles[p].debug_color;
			xpbd_particle_instances[p].inverse_mass = particles[p].s.inverse_mass;
		}

		renderer_->SetXPBDDebugParticleInstances(xpbd_particle_instances);
	}
}
