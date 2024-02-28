#pragma once

#include <vector>
#include <string>

namespace pmk
{
	class ConstitutiveModel
	{
	public:
		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() = 0;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() = 0;
	};

	struct PhysicsMaterial
	{
		uint32_t render_material;
		ConstitutiveModel* constitutive_model;
	};
}
