#pragma once

#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "common_constants.h"

namespace pmk
{
	struct XPBDParticle;
	struct RigidBodyParticleCollisionInfo;
	class XPBDParticleContext;
	class XPBDRigidBodyContext;

	class XPBDConstraint
	{
	public:
		// Solve a single iteration of the constraint and return particle's delta_x.
		// Particle context is not const so it can record rigid body collision data if necessary.
		virtual glm::vec3 Solve(
			XPBDParticleContext* p_context,
			const XPBDRigidBodyContext* rb_context,
			const std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL>& start_of_ranges,
			uint32_t particle_idx,
			float delta_time,
			uint32_t chunk_begin,
			uint32_t chunk_end) const = 0;

		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() = 0;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() = 0;
	};
}
