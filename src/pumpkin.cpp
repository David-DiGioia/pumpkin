#include "pumpkin.h"

#include "cmake_config.h"
#include "logger.h"

void Pumpkin::Initialize()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_ = glfwCreateWindow(width_, height_, "Pumpkin Engine", nullptr, nullptr);

	renderer_.Initialize(window_);
}

void Pumpkin::Start()
{
	logger::Print("Pumpkin Engine Version %d.%d\n\n", config::PUMPKIN_VERSION_MAJOR, config::PUMPKIN_VERSION_MINOR);

	Initialize();
	scene_.ImportGLTF(&renderer_, "../../assets/test_gltf.gltf");
	MainLoop();
	CleanUp();
}

void Pumpkin::HostWork()
{

}

void Pumpkin::HostRenderWork()
{
	scene_.UpdateRenderObjects();
}

void Pumpkin::MainLoop()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();

		HostWork();
		renderer_.WaitForLastFrame();
		HostRenderWork();
		scene_.DrawScene(renderer_);
	}
}

void Pumpkin::CleanUp()
{
	renderer_.CleanUp();
	glfwDestroyWindow(window_);
	glfwTerminate();
}
