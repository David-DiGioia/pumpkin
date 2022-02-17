#include "init.h"

#include "project_config.h"
#include "vulkan_util.h"
#include "logger.h"

namespace renderer
{
	void Context::Initialize()
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Pumpkin App";
		app_info.applicationVersion = 0;
		app_info.pEngineName = "Pumpkin Engine";
		app_info.engineVersion = PUMPKIN_VERSION_MAJOR * 1000 + PUMPKIN_VERSION_MINOR;
		app_info.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo instance_info{};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	}
}
