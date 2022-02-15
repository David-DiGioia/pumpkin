#include "vulkan_util.h"

#include "logger.h"

void Check(VkResult result)
{
	if (result != VK_SUCCESS) {
		logger::Error("Vulkan function returned the VkResult: %d", result);
	}
}
