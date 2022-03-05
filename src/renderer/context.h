#pragma once

#include <vector>
#include <string>
#include "volk.h"
#include "GLFW/glfw3.h"

#include "string_util.h"

namespace renderer
{
	// GLFW instance extensions are added later.
	const std::vector<const char*> required_instance_extensions{
		// Debug util extension can be ignored in GetRequiredExtensions depending on optimization level.
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
	};

	const std::vector<const char*> required_device_extensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // Needed by acceleration structure extension.
	};

	const std::vector<const char*> required_layers = {
		"VK_LAYER_KHRONOS_validation",
		//"VK_LAYER_KHRONOS_synchronization2",
	};

	class Context
	{
	public:
		void Initialize(GLFWwindow* glfw_window);

		void CleanUp();

		VkInstance instance{};
		GLFWwindow* window{};
		VkSurfaceKHR surface{};
		VkPhysicalDevice physical_device{};
		VkDevice device{};

	private:
		void InitializeInstance();

		void InitializeDebugMessenger();

		void InitializeSurface();

		void ChoosePhysicalDevice();

		void InitializeDevice();

		// Helper functions
		void CheckInstanceExtensionsSupported(const pmkutil::StringArray& requested_extensions);

		pmkutil::StringArray GetRequiredInstanceExtensions();

		void CheckValidationLayersSupported(const std::vector<const char*>& requested_layers);

		VkDeviceSize GetPhysicalDeviceRam(VkPhysicalDevice physical_device);

		uint32_t GetGraphicsQueueFamilyIndex();

		void CheckDeviceExtensionsSupported(const std::vector<const char*>& requested_extensions);

		VkDebugUtilsMessengerEXT debug_messenger_{};
	};
}
