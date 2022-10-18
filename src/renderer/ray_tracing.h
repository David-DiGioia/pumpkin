#pragma once

#include <vector>
#include "volk.h"

namespace renderer
{
	class RayTracingContext
	{
	public:
		void Initialize();

		void AddBlas();

		void BuildBlases();
	};
}
