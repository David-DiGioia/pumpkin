#pragma once

#include <vector>
#include <string>

namespace pmk
{
	struct XPBDParticle;
	struct XPBDContext;

	class XPBDConstraint
	{
	public:
		// Called once before any invocations to Solve() for that frame.
		virtual void Preprocess(const XPBDContext* context, float delta_time) = 0;

		virtual void Solve(const XPBDContext* context, uint32_t particle_idx, float delta_time) const = 0;

		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() = 0;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() = 0;
	};
}
