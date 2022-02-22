#include "init.h"

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
		void* p_user_data) {

		if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			logger::TaggedError("DebugCallback", logger::TextColor::YELLOW, "%s\n", p_callback_data->pMessage);
		}

		return VK_FALSE;
	}

	// Helper functions ----------------------------------------------------------------------------------------

	void CheckExtensionsSupported(const pmkutil::StringArray& requested_extensions)
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
		for (const std::string& requested : requested_extensions) {
			if (extension_property_set.find(requested) == extension_property_set.end()) {
				logger::Error("Extension %s is not supported on this machine.\n", requested.c_str());
			}
		}
	}

	pmkutil::StringArray GetRequiredExtensions()
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

		if (required_extensions.empty()) {
			logger::Print("\t[None]\n");
		}
		for (const char* s : required_extensions) {

			// If validation is disabled we don't enable debug utils extension.
			if (config::disable_validation) {
				if (std::string{ s } == VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
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

	void CheckValidationLayersSupported(const std::vector<const char*>& requested_layers)
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

	VkDeviceSize GetPhysicalDeviceRam(VkPhysicalDevice physical_device)
	{
		VkDeviceSize max_vram{ 0 };

		VkPhysicalDeviceMemoryProperties2 mem_prop{};
		mem_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		vkGetPhysicalDeviceMemoryProperties2(physical_device, &mem_prop);

		for (int i{ 0 }; i < mem_prop.memoryProperties.memoryHeapCount; ++i) {

			const auto& heap{ mem_prop.memoryProperties.memoryHeaps[i] };

			// Looking only at device local memory, find heap with maximum vram.
			if ((heap.size > max_vram) &&
				(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) {

				max_vram = heap.size;
			}
		}

		return max_vram;
	}

	VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice>& physical_devices)
	{
		VkPhysicalDevice max_physical_device{ 0 };
		VkDeviceSize max_vram{ 0 };
		std::string gpu_name{};

		for (auto physical_device : physical_devices)
		{
			VkPhysicalDeviceProperties2 prop{};
			prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			vkGetPhysicalDeviceProperties2(physical_device, &prop);

			VkDeviceSize current_vram{ GetPhysicalDeviceRam(physical_device) };

			if (current_vram > max_vram) {
				max_physical_device = physical_device;
				max_vram = current_vram;
				gpu_name = prop.properties.deviceName;
			}
		}

		logger::Print("Selecting %s with largest device-local heap of %.2f GB.\n", gpu_name.c_str(), max_vram / 1'000'000'000.0f);
		return max_physical_device;
	}

	// Main functions ------------------------------------------------------------------------------------------

	void Context::Initialize()
	{
		InitializeInstance();

		if (!config::disable_validation) {
			InitializeDebugMessenger();
		}

		InitializePhysicalDevice();
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
		pmkutil::StringArray extensions_string_array{ GetRequiredExtensions() };
		const char** extensions{ extensions_string_array.GetStringArray(&extension_count) };;
		CheckExtensionsSupported(extensions_string_array);

		uint32_t layer_count{ 0 };
		const char* const* layers{ nullptr };

		if (!config::disable_validation) {
			CheckValidationLayersSupported(required_layers);
			layer_count = (uint32_t)required_layers.size();
			layers = required_layers.data();
		}

		VkInstanceCreateInfo instance_info{};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_info.pApplicationInfo = &app_info;
		instance_info.enabledLayerCount = layer_count;
		instance_info.ppEnabledLayerNames = layers;
		instance_info.enabledExtensionCount = extension_count;
		instance_info.ppEnabledExtensionNames = extensions;

		VkResult result{ vkCreateInstance(&instance_info, nullptr, &instance_) };
		CheckResult(result, "Failed to create instance.");

		volkLoadInstance(instance_);
	}

	void Context::InitializeDebugMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
		messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messenger_info.pfnUserCallback = DebugCallback;
		messenger_info.pUserData = nullptr;

		VkResult result{ vkCreateDebugUtilsMessengerEXT(instance_, &messenger_info, nullptr, &debug_messenger_) };
		CheckResult(result, "Failed to create debug messenger.");
	}

	void Context::InitializePhysicalDevice()
	{
		uint32_t physical_device_count{};
		vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		vkEnumeratePhysicalDevices(instance_, &physical_device_count, physical_devices.data());

		physical_device_ = ChoosePhysicalDevice(physical_devices);
	}

	void Context::InitializeDevice()
	{

	}

	void Context::CleanUp()
	{
		if (!config::disable_validation) {
			vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
		}

		vkDestroyInstance(instance_, nullptr);
	}
}
