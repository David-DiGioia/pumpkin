#pragma once
#pragma once

#include <vector>
#include <optional>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "vulkan_renderer.h"
#include "constraint.h"

namespace pmk
{
	constexpr uint32_t MAX_COLLISION_PAIRS{ 8 }; // Maximum collision pairs between two voxel objects.

	class XPBDRigidBodyContext;

	class RigidBodyConstraint : public XPBDConstraint
	{
	public:
		RigidBodyConstraint();

		// Called once before any invocations to Solve() for that frame.
		virtual void Preprocess(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, float delta_time) override;

		// Solve a single iteration of the constraint and return delta_x.
		virtual glm::vec3 Solve(XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const override;

		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() override;
	};

	// Boundary condition for particles colliding with rigid bodies.
	enum class BoundaryCondition
	{
		STICKY,
		SLIP,
		SEPARATE,
	};

	struct Node;

	struct RigidBody
	{
		Node* node;                 // Rigid body transform is stored in node.
		float mass;                 // In kilograms.
		float dynamic_friction;     // Dimensionless.
		glm::vec3 center_of_mass;   // Relative to voxel coordinates.
		glm::mat3 inertia_tensor;   // In kilogram meter squared.
		glm::vec3 velocity;         // In meters per second.
		glm::vec3 angular_velocity; // In radians per second.
		bool immovable;
		BoundaryCondition boundary_condition;

		glm::vec3 previous_position;
		glm::quat previous_rotation;
		renderer::VoxelChunk voxel_chunk;

		glm::mat4 cached_world_transform{};
		glm::mat4 cached_inv_world_transform{};

		// Given voxel world space position, get the voxel coordinate from rb that collides with it.
		// Returns empty optional if no solution.
		std::optional<glm::uvec3> GetCollisionCoordinate(const glm::mat4& inv_world_transform, const glm::vec3& global_pos) const;

		// Convert voxel coordinate to world space position.
		glm::vec3 CoordinateToGlobal(const glm::mat4& world_transform, const glm::uvec3& voxel_coord) const;

		// Projects incompatible particle velocity to rigid body.
		glm::vec3 VelocityProject(const glm::vec3& particle_velocity, const glm::vec3& particle_normal, const glm::vec3& node_position) const;

		// Get the velocity of a rigid body at a given world space position.
		glm::vec3 WorldVelocity(const glm::vec3& world_position) const;

		void ApplyImpulse(const glm::vec3& impulse, const glm::vec3& point_of_application);
	};

	struct CollisionPair
	{
		glm::uvec3 coordinate_a; // Colliding voxel of object A.
		glm::uvec3 coordinate_b; // Colliding voxel of object B.
	};

	class Scene;
	struct PhysicsMaterial;

	class XPBDRigidBodyContext
	{
	public:
		void Initialize(renderer::VulkanRenderer* renderer, Scene* scene, const std::vector<PhysicsMaterial*>* physics_materials);

		void CleanUp();

		void PhysicsUpdate(float delta_time);

		void UpdateFromParticles(float delta_time, XPBDParticleContext* p_context);

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		bool GetPhysicsUpdateEnabled() const;

		void ResetRigidBodies();

		const std::vector<RigidBody*>& GetRigidBodies() const;

		std::vector<RigidBody*>& GetRigidBodies();

		// Populate rigid_bodies_ with rigid bodies made from connected voxels sharing
		// the same rigid body physics material.
		// Removes the rigid body voxels from input voxels.
		// Returns list of node indices/IDs created from rigid bodies.
		std::vector<uint32_t> CreateRigidBodiesByConnectedness(renderer::VoxelChunk& voxel_chunk, bool* out_is_empty);

		std::array<CollisionPair, MAX_COLLISION_PAIRS> ComputeCollisionPairs(const RigidBody* a, const RigidBody* b, uint32_t* out_count) const;

		// If collision occurs then return world position of rigid body voxel that p collides with. Empty optional means no collision occurred.
		std::optional<glm::vec3> ComputeParticleCollision(const RigidBody* rb, const glm::vec3& particle_position) const;

#ifdef EDITOR_ENABLED
		void SetRigidBodyOverlayEnabled(bool enabled);

		void GenerateDynamicDebugRbVoxelInstances() const;
#endif

	private:
		void SolvePositions(float h);

		void RigidBodyFloodFill(const glm::uvec3& coordinate, renderer::VoxelChunk& voxel_chunk, const std::vector<uint8_t>& material_mask);

		float GetVoxelMass(uint32_t physics_material_index) const;

		glm::mat3 ComputeInertiaTensor(const renderer::VoxelChunk& voxel_chunk, const glm::vec3& center_of_mass);

		void CreateRigidBody(
			const glm::uvec3& min_extents,
			const glm::uvec3& max_extents,
			std::vector<std::pair<renderer::Voxel, glm::uvec3>>&& voxel_pairs,
			glm::vec3&& center_of_mass,
			float mass);

		renderer::VulkanRenderer* renderer_{};
		Scene* scene_{};
		std::vector<RigidBody*> rigid_bodies_{};
		bool update_physics_{};
#ifdef EDITOR_ENABLED
		bool generate_rb_voxel_instances_{};
#endif

		const std::vector<PhysicsMaterial*>* physics_materials_{};
	};
}
