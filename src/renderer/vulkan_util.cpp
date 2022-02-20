#include "vulkan_util.h"

#include "logger.h"

void CheckResult(VkResult result, const std::string& msg)
{
	if (result != VK_SUCCESS) {
		logger::Error("Vulkan function returned the VkResult: %d\nMessage: %s\n", result, msg.c_str());
	}
}
