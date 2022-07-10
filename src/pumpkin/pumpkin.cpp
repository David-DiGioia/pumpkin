#include "pumpkin.h"

#include "cmake_config.h"
#include "logger.h"

namespace pmk
{
	void Pumpkin::Initialize()
	{
		logger::Print("Pumpkin Engine Version %d.%d\n\n", config::PUMPKIN_VERSION_MAJOR, config::PUMPKIN_VERSION_MINOR);

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window_ = glfwCreateWindow(width_, height_, "Pumpkin Engine", nullptr, nullptr);

		renderer_.Initialize(window_);
		scene_.Initialize(&renderer_);
	}

	void Pumpkin::Start()
	{
		MainLoop();
	}

	void Pumpkin::HostWork()
	{
	}

	void Pumpkin::HostRenderWork()
	{
		scene_.UploadRenderObjects();
		scene_.UploadCamera();
	}

	void Pumpkin::MainLoop()
	{
		while (!glfwWindowShouldClose(window_))
		{
			glfwPollEvents();

			UpdateDeltaTime();
			HostWork();
			renderer_.WaitForLastFrame();
			HostRenderWork();
			renderer_.Render();
		}
	}

	void Pumpkin::CleanUp()
	{
		scene_.CleanUp();
		renderer_.CleanUp();
		glfwDestroyWindow(window_);
		glfwTerminate();
	}

	void Pumpkin::SetImGuiCallbacksInfo(const renderer::ImGuiCallbacks& editor_info)
	{
#ifdef EDITOR_ENABLED
		renderer_.SetImGuiCallbacks(editor_info);
#endif
	}

	void Pumpkin::SetEditorViewportSize(const renderer::Extent& extent)
	{
#ifdef EDITOR_ENABLED
		renderer_.SetImGuiViewportSize(extent);
#endif
	}

	void Pumpkin::SetCameraPosition(const glm::vec3& pos)
	{
		scene_.GetCamera().position = pos;
	}

	Scene& Pumpkin::GetScene()
	{
		return scene_;
	}

	float Pumpkin::GetDeltaTime() const
	{
		return delta_time_;
	}

	void Pumpkin::UpdateDeltaTime()
	{
		auto current_time{ std::chrono::steady_clock::now() };
		auto microseconds{ std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_time_).count() };
		delta_time_ = microseconds / 1'000'000.0f;
		last_time_ = current_time;
	}
}
