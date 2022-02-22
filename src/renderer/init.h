#pragma once

#include <vector>
#include <string>
#include "volk.h"

#include "string_util.h"

namespace renderer
{
	const std::vector<const char*> required_extensions{
		// Debug util extension can be ignored in GetRequiredExtensions depending on optimization level.
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	const std::vector<const char*> required_layers = {
		"VK_LAYER_KHRONOS_validation",
		//"VK_LAYER_KHRONOS_synchronization2",
	};

	class Context
	{
	public:
		void Initialize();

		void InitializeInstance();

		void InitializeDebugMessenger();

		void InitializePhysicalDevice();

		void InitializeDevice();

		void CleanUp();

	private:
		VkInstance instance_{};
		VkDebugUtilsMessengerEXT debug_messenger_{};
		VkPhysicalDevice physical_device_{};
	};
}
