#pragma once

#include <vector>
#include "glm/glm.hpp"

#include "vulkan_renderer.h"
#include "constitutive_model.h"

namespace pmk
{
	class RigidBodyModel : public ConstitutiveModel
	{
	public:
		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() override;
	};

	struct Node;

	struct RigidBody
	{
		Node* node;               // Rigid body transform is stored in node.
		float mass;               // In kilograms.
		glm::vec3 center_of_mass; // Relative to voxel coordinates.

		renderer::VoxelChunk voxel_chunk;
	};

	class Scene;

	class RigidBodyContext
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer, Scene* scene, const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		// Populate rigid_bodies_ with rigid bodies made from connected voxels sharing
		// the same rigid body physics material.
		// Removes the rigid body voxels from input voxels.
		void CreateRigidBodiesByConnectedness(renderer::VoxelChunk& voxel_chunk);

	private:
		void RigidBodyFloodFill(const glm::uvec3& coordinate, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask);

		float GetVoxelMass(uint32_t physics_material_index) const;

		void CreateRigidBody(
			const glm::uvec3& min_extents,
			const glm::uvec3& max_extents,
			std::vector<std::pair<renderer::Voxel, glm::uvec3>>&& voxel_pairs,
			glm::vec3&& center_of_mass,
			float mass);

		renderer::VulkanRenderer* renderer_{};
		Scene* scene_;
		std::vector<RigidBody*> rigid_bodies_{};

		const std::vector<PhysicsMaterial*>* physics_materials_{};
	};
}
