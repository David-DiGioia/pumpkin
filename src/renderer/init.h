#pragma once

#include <vector>
#include <string>
#include "volk.h"

#include "string_util.h"

namespace renderer
{
	const std::vector<const char*> required_extensions{};

	const std::vector<const char*> required_layers = {
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_KHRONOS_tetris",
		//"VK_LAYER_KHRONOS_synchronization2",
	};

	class Context
	{
	public:
		void Initialize();

		void InitializeInstance();

		void InitializePhysicalDevice();

		void InitializeDevice();

		void CleanUp();

	private:
		VkInstance instance_{};
	};
}
