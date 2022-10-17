#pragma once

#include <vector>
#include "volk.h"

namespace renderer
{
	class RayTracingContext
	{
		void AddBlas();

		void BuildBlases();
	};
}
