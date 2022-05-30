#include "pumpkin.h"

#include "cmake_config.h"
#include "logger.h"

namespace pmk
{
	void Pumpkin::Initialize()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window_ = glfwCreateWindow(width_, height_, "Pumpkin Engine", nullptr, nullptr);

		renderer_.Initialize(window_);
		scene_.Initialize(&renderer_);
	}

	void Pumpkin::Start()
	{
		logger::Print("Pumpkin Engine Version %d.%d\n\n", config::PUMPKIN_VERSION_MAJOR, config::PUMPKIN_VERSION_MINOR);

#ifdef EDITOR_ENABLED
		logger::Print("Editor enabled in Pumpkin\n");
#else
		logger::Print("Editor disabled in Pumpkin\n");
#endif

		Initialize();
		scene_.ImportGLTF("../../../assets/test_gltf.gltf");
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
			renderer_.Render();
		}
	}

	void Pumpkin::CleanUp()
	{
		renderer_.CleanUp();
		glfwDestroyWindow(window_);
		glfwTerminate();
	}

	void Pumpkin::SetEditorInfo(const renderer::EditorInfo& editor_info)
	{
		editor_mode_enabled_ = true;
		renderer_.SetEditorInfo(editor_info);
	}

	void Pumpkin::SetEditorViewportSize(const renderer::Extent& extent)
	{
		renderer_.SetEditorViewportSize(extent);
	}
}
