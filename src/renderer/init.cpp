#include "init.h"

#include "GLFW/glfw3.h"

#include "project_config.h"
#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	StringArray GetRequiredExtensions()
	{
		StringArray string_array{};

		uint32_t glfw_extension_count{};
		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		string_array.PushBack(glfw_extensions, glfw_extension_count);

		logger::Print("Enabling the following GLFW extensions:\n");

		for (uint32_t i{ 0 }; i < glfw_extension_count; ++i) {
			logger::Print("\t%s\n", glfw_extensions[i]);
		}

		logger::Print("Enabling the following application-requested extensions:\n");

		if (required_extensions.empty()) {
			logger::Print("\t[None]\n");
		}

		for (const std::string& s : required_extensions) {
			logger::Print("\t%s\n", s.c_str());
			string_array.PushBack(s);
		}

		return string_array;
	}

	void Context::Initialize()
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Pumpkin App";
		app_info.applicationVersion = 1;
		app_info.pEngineName = "Pumpkin Engine";
		app_info.engineVersion = PUMPKIN_VERSION_MAJOR * 1000 + PUMPKIN_VERSION_MINOR;
		app_info.apiVersion = VK_API_VERSION_1_3;

		uint32_t extension_count{};
		StringArray extensions_string_array{ GetRequiredExtensions() };
		const char** extensions{ extensions_string_array.GetStringArray(&extension_count) };;

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
}
