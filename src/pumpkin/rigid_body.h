#pragma once

#include <vector>
#include "glm/glm.hpp"

#include "vulkan_renderer.h"
#include "constitutive_model.h"

namespace pmk
{
	class RigidBodyConstitutiveModel : public ConstitutiveModel
	{
	public:

	};

	struct RigidBodyVoxel
	{
		glm::uvec3 coordinate;
		uint8_t physics_material_index;
	};

	struct Node;

	struct RigidBody
	{
		Node* node;               // Rigid body transform is stored in node.
		glm::vec3 center_of_mass; // Relative to voxel coordinates.
		std::vector<RigidBodyVoxel> voxels;
	};

	class Scene;

	class RigidBodyContext
	{
	public:
		void Initialize(Scene* scene);

		void CleanUp();

		// Populate rigid_bodies_ with rigid bodies made from connected static particles sharing
		// the same rigid body physics material.
		// Removes the rigid body particles from input particles.
		void CreateRigidBodiesByConnectedness(std::vector<renderer::StaticParticle>& particles);

	private:
		Scene* scene_;
		std::vector<RigidBody*> rigid_bodies_{};
	};
}
