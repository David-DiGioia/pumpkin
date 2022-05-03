#include "context.h"

#include <unordered_set>
#include "GLFW/glfw3.h"

#include "cmake_config.h"
#include "project_config.h"
#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	// Debug callback ------------------------------------------------------------------------------------------

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		void* p_user_data)
	{
		if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			logger::TaggedWarning("DebugCallback", logger::TextColor::BRIGHT_BLACK, "%s\n", p_callback_data->pMessage);
		}
		else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			logger::TaggedError("DebugCallback", logger::TextColor::BRIGHT_BLACK, "%s\n", p_callback_data->pMessage);
		}

		return VK_FALSE;
	}

	// Main functions ------------------------------------------------------------------------------------------

	void Context::Initialize(GLFWwindow* glfw_window)
	{
		window = glfw_window;

		InitializeInstance();
		InitializeDebugMessenger();
		InitializeSurface();
		ChoosePhysicalDevice();
		InitializeDevice();
	}

	void Context::InitializeInstance()
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Pumpkin App";
		app_info.applicationVersion = 1;
		app_info.pEngineName = "Pumpkin Engine";
		app_info.engineVersion = config::PUMPKIN_VERSION_MAJOR * 1000 + config::PUMPKIN_VERSION_MINOR;
		app_info.apiVersion = VK_API_VERSION_1_3;

		uint32_t extension_count{};
		pmkutil::StringArray extensions_string_array{ GetRequiredInstanceExtensions() };
		const char** extensions{ extensions_string_array.GetStringArray(&extension_count) };;
		CheckInstanceExtensionsSupported(extensions_string_array);

		uint32_t layer_count{ 0 };
		const char* const* layers{ nullptr };

		if (!config::disable_validation) {
			CheckValidationLayersSupported(required_layers);
			layer_count = (uint32_t)required_layers.size();
			layers = required_layers.data();
		}

		VkInstanceCreateInfo instance_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app_info,
			.enabledLayerCount = layer_count,
			.ppEnabledLayerNames = layers,
			.enabledExtensionCount = extension_count,
			.ppEnabledExtensionNames = extensions,
		};

		VkResult result{ vkCreateInstance(&instance_info, nullptr, &instance) };
		CheckResult(result, "Failed to create instance.");

		volkLoadInstance(instance);
	}

	void Context::InitializeDebugMessenger()
	{
		if (config::disable_validation) {
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT messenger_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = nullptr,
		};

		VkResult result{ vkCreateDebugUtilsMessengerEXT(instance, &messenger_info, nullptr, &debug_messenger_) };
		CheckResult(result, "Failed to create debug messenger.");
	}

	void Context::InitializeSurface()
	{
		VkResult result{ glfwCreateWindowSurface(instance, window, nullptr, &surface) };
		CheckResult(result, "Failed to create surface.");
	}

	void Context::ChoosePhysicalDevice()
	{
		uint32_t physical_device_count{};
		vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

		VkPhysicalDevice max_physical_device{ 0 };
		VkDeviceSize max_vram{ 0 };
		std::string gpu_name{};

		for (auto physical_device : physical_devices)
		{
			VkPhysicalDeviceProperties2 prop{};
			prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			vkGetPhysicalDeviceProperties2(physical_device, &prop);
			VkDeviceSize current_vram{ GetPhysicalDeviceRam(physical_device) };

			VkPhysicalDeviceMemoryProperties mem_prop{};
			vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_prop);

			if (current_vram > max_vram)
			{
				max_physical_device = physical_device;
				max_vram = current_vram;
				gpu_name = prop.properties.deviceName;
			}
		}

		logger::Print("Selecting %s with largest device-local heap of %.2f GB.\n\n", gpu_name.c_str(), max_vram / 1'000'000'000.0f);

		physical_device = max_physical_device;
	}

	void Context::InitializeDevice()
	{
		float priority{ 1.0f };

		graphics_queue_family_ = ChooseGraphicsQueueFamilyIndex();

		VkDeviceQueueCreateInfo graphics_queue_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = graphics_queue_family_,
			.queueCount = 1,
			.pQueuePriorities = &priority,
		};

		logger::Print("Enabling the following device extensions:\n");
		for (const char* extension : required_device_extensions) {
			logger::Print("\t%s\n", extension);
		}
		logger::Print("\n");

		CheckDeviceExtensionsSupported(required_device_extensions);

		VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering = VK_TRUE,
		};

		VkPhysicalDeviceFeatures features{};

		VkDeviceCreateInfo device_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &dynamic_rendering_features,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &graphics_queue_info,
			.enabledExtensionCount = (uint32_t)required_device_extensions.size(),
			.ppEnabledExtensionNames = required_device_extensions.data(),
			.pEnabledFeatures = &features,
		};

		VkResult result{ vkCreateDevice(physical_device, &device_info, nullptr, &device) };
		CheckResult(result, "Failed to create device.");

		volkLoadDevice(device);

		vkGetDeviceQueue(device, graphics_queue_family_, 0, &graphics_queue);
	}

	void Context::CleanUp()
	{
		vkDestroyDevice(device, nullptr);

		if (!config::disable_validation) {
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger_, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);

		// Physical device is implicitly destroyed with the instance.
		vkDestroyInstance(instance, nullptr);
	}

	Extent Context::GetWindowExtent()
	{
		int width{}, height{};
		glfwGetFramebufferSize(window, &width, &height);
		return { (uint32_t)width, (uint32_t)height };
	}

	uint32_t Context::GetGraphicsQueueFamilyIndex()
	{
		return graphics_queue_family_;
	}



	// Helper functions ----------------------------------------------------------------------------------------

	void Context::CheckInstanceExtensionsSupported(const pmkutil::StringArray& requested_extensions)
	{
		// Get supported extension properties.
		uint32_t extension_properties_count{ 0 };
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_properties_count, nullptr);
		std::vector<VkExtensionProperties> extension_properties(extension_properties_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_properties_count, extension_properties.data());

		// Convert to set.
		std::unordered_set<std::string> extension_property_set;

		for (const auto& prop : extension_properties) {
			extension_property_set.insert(prop.extensionName);
		}

		// Check that each requested extension is supported.
		for (const std::string& requested : requested_extensions)
		{
			if (extension_property_set.find(requested) == extension_property_set.end()) {
				logger::Error("Instance extension %s is not supported on this machine.\n", requested.c_str());
			}
		}
	}

	pmkutil::StringArray Context::GetRequiredInstanceExtensions()
	{
		pmkutil::StringArray string_array{};

		// GLFW extensions.
		uint32_t glfw_extension_count{};
		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		logger::Print("Enabling the following GLFW extensions:\n");
		for (uint32_t i{ 0 }; i < glfw_extension_count; ++i) {
			logger::Print("\t%s\n", glfw_extensions[i]);
		}

		string_array.PushBack(glfw_extensions, glfw_extension_count);

		// Extensions requested by renderer.
		logger::Print("Enabling the following application-requested extensions:\n");

		if (required_instance_extensions.empty()) {
			logger::Print("\t[None]\n");
		}

		for (const char* s : required_instance_extensions)
		{
			// If validation is disabled we don't enable debug utils extension.
			if (config::disable_validation)
			{
				if (std::string{ s } == VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
				{
					logger::Print("\t[Ignored] %s\n", s);
					continue;
				}
			}

			logger::Print("\t%s\n", s);
			string_array.PushBack(s);
		}

		logger::Print("\n");

		return string_array;
	}

	void Context::CheckValidationLayersSupported(const std::vector<const char*>& requested_layers)
	{
		uint32_t layer_count{};
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		std::vector<VkLayerProperties> available_layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		std::unordered_set<std::string> layers_set{};

		for (const VkLayerProperties& prop : available_layers) {
			layers_set.insert(prop.layerName);
		}

		for (const char* layer : required_layers) {
			if (layers_set.find(layer) == layers_set.end()) {
				logger::Error("Unable to find required layer %s\n", layer);
			}
		}
	}

	VkDeviceSize Context::GetPhysicalDeviceRam(VkPhysicalDevice physical_device)
	{
		VkDeviceSize max_vram{ 0 };

		VkPhysicalDeviceMemoryProperties2 mem_prop{};
		mem_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		vkGetPhysicalDeviceMemoryProperties2(physical_device, &mem_prop);

		for (uint32_t i{ 0 }; i < mem_prop.memoryProperties.memoryHeapCount; ++i) {

			const auto& heap{ mem_prop.memoryProperties.memoryHeaps[i] };

			// Looking only at device local memory, find heap with maximum vram.
			if ((heap.size > max_vram) &&
				(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) {

				max_vram = heap.size;
			}
		}

		return max_vram;
	}

	uint32_t Context::ChooseGraphicsQueueFamilyIndex()
	{
		uint32_t prop_count{};
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &prop_count, nullptr);
		std::vector<VkQueueFamilyProperties2> properties(prop_count);
		for (auto& prop : properties) {
			prop.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		}
		vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &prop_count, properties.data());

		uint32_t graphics_family{};

		for (uint32_t i{ 0 }; i < prop_count; ++i)
		{
			bool graphics_support{ (bool)(properties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) };
			VkBool32 present_support{ false };
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);

			// All graphics queue families also support transfer operations
			// whether or not that bit is specified.
			if (graphics_support && present_support)
			{
				graphics_family = i;
				//break;
			}
		}

		return graphics_family;
	}

	void Context::CheckDeviceExtensionsSupported(const std::vector<const char*>& requested_extensions)
	{
		// Get supported extension properties.
		uint32_t extension_properties_count{ 0 };
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_properties_count, nullptr);
		std::vector<VkExtensionProperties> extension_properties(extension_properties_count);
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_properties_count, extension_properties.data());

		// Convert to set.
		std::unordered_set<std::string> extension_property_set;

		for (const auto& prop : extension_properties) {
			extension_property_set.insert(prop.extensionName);
		}

		// Check that each requested extension is supported.
		for (const std::string& requested : requested_extensions) {
			if (extension_property_set.find(requested) == extension_property_set.end()) {
				logger::Error("Device extension %s is not supported on this machine.\n", requested.c_str());
			}
		}
	}

}
