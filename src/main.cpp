#include "project_config.h"

#include "GLFW/glfw3.h"

#include "vulkan_renderer.h"
#include "logger.h"

int main()
{
	logger::Print("Pumpkin Engine Version %d.%d\n", PUMPKIN_VERSION_MAJOR, PUMPKIN_VERSION_MINOR);

	VulkanRenderer renderer{};

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window{ glfwCreateWindow(800, 600, "Pumpkin Engine", nullptr, nullptr) };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
	
	return 0;
}
