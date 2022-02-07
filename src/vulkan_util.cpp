#include "vulkan_util.h"

#include "log.h"

void Check(VkResult result)
{
	if (result != VK_SUCCESS) {
		logger::Error("Vulkan function returned the VkResult: %d", result);
	}
}
