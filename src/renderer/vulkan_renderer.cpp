#include "vulkan_renderer.h"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

#include "vulkan_util.h"
#include "logger.h"
#include "init.h"

namespace renderer
{
    void VulkanRenderer::Initialize()
    {
        VkResult result{ volkInitialize() };
        CheckResult(result, "Failed to initialize volk.");

        context_.Initialize();
    }

    void VulkanRenderer::CleanUp()
    {
        context_.CleanUp();
    }
}
