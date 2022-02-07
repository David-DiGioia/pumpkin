#include "project_config.h"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "GLFW/glfw3.h"

#include "log.h"
#include "vulkan_util.h"

int main() {
    Check(volkInitialize());

    logger::Print("Vulkan Ray Tracing Version %d.%d\n", VRT_VERSION_MAJOR, VRT_VERSION_MINOR);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window{ glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr) };

    uint32_t extensionCount{ 0 };
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    logger::Print("%d extensions supported\n", extensionCount);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}
