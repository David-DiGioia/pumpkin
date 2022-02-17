#include "pumpkin.h"

#include "project_config.h"
#include "logger.h"

void Pumpkin::Initialize()
{
    width_ = 800;
    height_ = 600;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(width_, height_, "Pumpkin Engine", nullptr, nullptr);

    renderer_.Initialize();
}

void Pumpkin::Start()
{
	logger::Print("Pumpkin Engine Version %d.%d\n", PUMPKIN_VERSION_MAJOR, PUMPKIN_VERSION_MINOR);

    Initialize();
    MainLoop();
    CleanUp();
}

void Pumpkin::MainLoop()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}

void Pumpkin::CleanUp()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}
