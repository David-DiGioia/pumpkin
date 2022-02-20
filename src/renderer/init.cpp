#include "init.h"

#include <unordered_set>
#include "GLFW/glfw3.h"

#include "cmake_config.h"
#include "project_config.h"
#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	
	void CheckExtensionsSupported(const StringArray& requested_extensions)
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

	StringArray GetRequiredExtensions()
	{
		StringArray string_array{};

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
			logger::Print("\t%s\n", s);
		}

		string_array.PushBack(required_extensions.data(), (uint32_t)required_extensions.size());

		return string_array;
	}

	void CheckValidationLayersSupported(const std::vector<const char*>& requested_layers)
	{

	}

	void Context::Initialize()
	{
		InitializeInstance();
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
		StringArray extensions_string_array{ GetRequiredExtensions() };
		const char** extensions{ extensions_string_array.GetStringArray(&extension_count) };;
		CheckExtensionsSupported(extensions_string_array);

		uint32_t layer_count{};
		const char* const* layers{ nullptr };

		if (config::optimization_level != config::OptimizationLevel::AGGRESSIVE) {
			CheckValidationLayersSupported(required_layers);
			layer_count = (uint32_t)required_layers.size();
			layers = required_layers.data();
		}

		VkInstanceCreateInfo instance_info{};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_info.pApplicationInfo = &app_info;
		instance_info.enabledLayerCount = 0;
		instance_info.ppEnabledLayerNames = nullptr;
		instance_info.enabledExtensionCount = extension_count;
		instance_info.ppEnabledExtensionNames = extensions;

		VkResult result{ vkCreateInstance(&instance_info, nullptr, &instance_) };
		CheckResult(result, "Failed to create instance.");
	}

	void Context::InitializePhysicalDevice()
	{

	}

	void Context::InitializeDevice()
	{

	}

	void Context::CleanUp()
	{
		vkDestroyInstance(instance_, nullptr);
	}
}
