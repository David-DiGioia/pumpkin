#pragma once

#include <vector>
#include <string>
#include "glm/glm.hpp"

namespace pmk
{
	struct XPBDParticle;
	class XPBDParticleContext;

	class XPBDConstraint
	{
	public:
		// Called once before any invocations to Solve() for that frame.
		virtual void Preprocess(const XPBDParticleContext* context, float delta_time) = 0;

		// Solve a single iteration of the constraint and return delta_x.
		virtual glm::vec3 Solve(const XPBDParticleContext* context, uint32_t particle_idx, float delta_time) const = 0;

		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() = 0;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() = 0;
	};
}
