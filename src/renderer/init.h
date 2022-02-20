#pragma once

#include <vector>
#include <string>
#include "volk.h"

#include "string_util.h"

namespace renderer
{
	const std::vector<std::string> required_extensions{};

	StringArray GetRequiredExtensions();

	class Context
	{
	public:
		void Initialize();

	private:
		VkInstance instance_{};
	};
}
