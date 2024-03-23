#pragma once
#pragma once

#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "vulkan_renderer.h"
#include "constitutive_model.h"

namespace pmk
{
	constexpr uint32_t MAX_COLLISION_PAIRS{ 4 }; // Maximum collision pairs between two voxel objects.

	class RigidBodyModel : public ConstitutiveModel
	{
	public:
		RigidBodyModel();

		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() override;
	};

	struct Node;

	struct RigidBody
	{
		Node* node;                 // Rigid body transform is stored in node.
		float mass;                 // In kilograms.
		glm::vec3 center_of_mass;   // Relative to voxel coordinates.
		glm::mat3 inertia_tensor;   // In kilogram meter squared.
		glm::vec3 velocity;         // In meters per second.
		glm::vec3 angular_velocity; // In radians per second.
		bool immovable;

		glm::vec3 previous_position;
		glm::quat previous_rotation;
		renderer::VoxelChunk voxel_chunk;
	};

	struct CollisionPair
	{
		glm::uvec3 coordinate_a; // Colliding voxel of object A.
		glm::uvec3 coordinate_b; // Colliding voxel of object B.
	};

	class Scene;

	class RigidBodyContext
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer, Scene* scene, const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetRigidBodies();

		// Populate rigid_bodies_ with rigid bodies made from connected voxels sharing
		// the same rigid body physics material.
		// Removes the rigid body voxels from input voxels.
		void CreateRigidBodiesByConnectedness(renderer::VoxelChunk& voxel_chunk);

		std::array<CollisionPair, MAX_COLLISION_PAIRS> ComputeCollisionPairs(const RigidBody* a, const RigidBody* b, uint32_t* out_count) const;

	private:
		void SolvePositions(float h);

		void RigidBodyFloodFill(const glm::uvec3& coordinate, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask);

		float GetVoxelMass(uint32_t physics_material_index) const;

		// Convert voxel coordinate to world space position.
		glm::vec3 CoordinateToGlobal(const RigidBody& rb, const glm::uvec3& voxel_coord) const;

		// Given voxel world space position, get the voxel coordinate from rb that collides with it.
		// Returns UINT_MAX if no solution.
		glm::uvec3 GetCollisionCoordinate(const RigidBody& rb, const glm::vec3& global_pos) const;

		glm::mat3 ComputeInertiaTensor(const renderer::VoxelChunk& voxel_chunk, const glm::vec3& center_of_mass);

		void CreateRigidBody(
			const glm::uvec3& min_extents,
			const glm::uvec3& max_extents,
			std::vector<std::pair<renderer::Voxel, glm::uvec3>>&& voxel_pairs,
			glm::vec3&& center_of_mass,
			float mass);

		// Copy all the members of a rigid body except temporary variables and voxel chunk.
		void CopyRigidBodyAttributes(RigidBody* from, RigidBody* to) const;

		renderer::VulkanRenderer* renderer_{};
		Scene* scene_;
		std::vector<RigidBody*> rigid_bodies_{};
		std::vector<RigidBody*> rigid_bodies_initial_{}; // Original state of all rigid bodies to allow resetting simulation.
		bool update_physics_{};
		bool is_reset_{}; // True if scene has been reset and play button has not been pressed since then.

		const std::vector<PhysicsMaterial*>* physics_materials_{};
	};
}
