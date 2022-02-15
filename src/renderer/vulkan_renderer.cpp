#include "vulkan_renderer.h"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

#include "vulkan_util.h"
#include "logger.h"

VulkanRenderer::VulkanRenderer()
{
    Check(volkInitialize());

    uint32_t extensionCount{ 0 };
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    logger::Print("%d extensions supported\n", extensionCount);

}

VulkanRenderer::~VulkanRenderer()
{

}
