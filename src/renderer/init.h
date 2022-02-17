#pragma once

#include "volk.h"

namespace renderer
{
	class Context
	{
	public:
		void Initialize();

	private:
		VkInstance instance_{};
	};
}
