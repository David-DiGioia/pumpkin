#pragma once

#include <vector>
#include <string>

namespace pmk
{
	class PhysicsContext;

	class ConstitutiveModel
	{
	public:
		// For updating the parameters from the UI for making physics materials in the editor.
		virtual std::vector<std::pair<float*, std::string>> GetParameters() = 0;

		// Should be called after any parameters from GetParameters() are mutated.
		virtual void OnParametersMutated() = 0;

		float GetDensity() const;

	protected:
		friend PhysicsContext;

		float density_;
	};

	struct PhysicsMaterial
	{
		uint32_t render_material;
		ConstitutiveModel* constitutive_model; // This is the owner of constitutive_model; there is no separate vector of constitutive models.
	};
}
