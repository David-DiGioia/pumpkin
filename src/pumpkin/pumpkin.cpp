#include "pumpkin.h"

#include "cmake_config.h"
#include "logger.h"

namespace pmk
{
	constexpr float PHYSICS_UPDATE_TIME{ 1.0f / 60.0f };

	void Pumpkin::Initialize()
	{
		logger::Print("Pumpkin Engine Version %d.%d\n\n", config::PUMPKIN_VERSION_MAJOR, config::PUMPKIN_VERSION_MINOR);

		if (glfwInit() != GLFW_TRUE) {
			logger::Error("Error initializing GLFW.");
		}
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
		physics_time_accumulator_ += delta_time_;
		if (physics_time_accumulator_ >= PHYSICS_UPDATE_TIME)
		{
			renderer_.ParticleUpdate(PHYSICS_UPDATE_TIME);
			physics_time_accumulator_ = 0.0f;
		}
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
			renderer_.BuildTlasAndUpdateBlases();
			renderer_.ComputeWork();
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

	renderer::Mesh* Pumpkin::GetMesh(renderer::RenderObjectHandle render_object)
	{
		return renderer_.GetMesh(render_object);
	}

	const std::vector<int>& Pumpkin::GetMaterialIndices(renderer::RenderObjectHandle render_object)
	{
		return renderer_.GetMaterialIndices(render_object);
	}

	std::vector<renderer::Material*>& Pumpkin::GetMaterials()
	{
		return renderer_.GetMaterials();
	}

	void Pumpkin::UpdateMaterials()
	{
		renderer_.UpdateMaterials();
	}

	renderer::Material* Pumpkin::MakeMaterialUnique(uint32_t material_index)
	{
		return renderer_.MakeMaterialUnique(material_index);
	}

	void Pumpkin::SetMaterialIndex(renderer::RenderObjectHandle render_object, uint32_t geometry_index, int material_index)
	{
		renderer_.SetMaterialIndex(render_object, geometry_index, material_index);
	}

	void Pumpkin::UpdateObjectBuffers()
	{
		renderer_.UpdateObjectBuffers();
	}

	float Pumpkin::GetDeltaTime() const
	{
		return delta_time_;
	}

	void Pumpkin::DumpRenderData(
		nlohmann::json& j,
		const std::filesystem::path& vertex_path,
		const std::filesystem::path& index_path,
		const std::filesystem::path& texture_path)
	{
		renderer_.DumpRenderData(j, vertex_path, index_path, texture_path);
	}

	void Pumpkin::LoadRenderData(
		nlohmann::json& j,
		const std::filesystem::path& vertex_path,
		const std::filesystem::path& index_path,
		const std::filesystem::path& texture_path,
		std::vector<int>* out_material_indices)
	{
		renderer_.LoadRenderData(j, vertex_path, index_path, texture_path, out_material_indices);
	}

	void Pumpkin::ClearOutlineSets()
	{
		renderer_.ClearOutlineSets();
	}

	void Pumpkin::AddOutlineSet(const std::vector<renderer::RenderObjectHandle>& selection_set, const glm::vec4& color)
	{
		renderer_.AddOutlineSet(selection_set, color);
	}

	void Pumpkin::QueueRaycast(const glm::vec3& origin, const glm::vec3& direction)
	{
		queued_raycasts_.push_back({ glm::vec4{origin, 0.0f}, glm::vec4{direction, 0.0f} });
	}

	std::vector<pmk::Rayhit> Pumpkin::CastQueuedRays()
	{
		std::vector<renderer::Rayhit> renderer_rayhits{ renderer_.CastRays(queued_raycasts_) };
		queued_raycasts_.clear();

		// Convert to pumpkin rayhits.
		std::vector<pmk::Rayhit> pmk_rayhits{};
		pmk_rayhits.reserve(renderer_rayhits.size());
		for (const renderer::Rayhit& renderer_rayhit : renderer_rayhits)
		{
			pmk::Rayhit& pmk_rayhit{ pmk_rayhits.emplace_back() };

			pmk_rayhit.node = GetNodeByRenderObject((renderer::RenderObjectHandle)renderer_rayhit.instance_index);
			pmk_rayhit.position = renderer_rayhit.position;
		}

		return pmk_rayhits;
	}

	Node* Pumpkin::GetNodeByRenderObject(renderer::RenderObjectHandle handle)
	{
		return scene_.GetNodeByRenderObject(handle);
	}

	void Pumpkin::AddRenderObjectToNode(Node* node, renderer::RenderObjectHandle handle)
	{
		scene_.AddRenderObjectToNode(node, handle);
	}

	uint32_t Pumpkin::CreateTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels, bool color_data)
	{
		return renderer_.CreateTexture(data, width, height, channels, color_data);
	}

	uint32_t Pumpkin::GetTextureCount() const
	{
		return renderer_.GetTextureCount();
	}

	void Pumpkin::ImportShader(const std::filesystem::path& spirv_path)
	{
		renderer_.ImportShader(spirv_path);
	}

	void Pumpkin::UpdateDeltaTime()
	{
		auto current_time{ std::chrono::steady_clock::now() };
		auto microseconds{ std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_time_).count() };
		delta_time_ = microseconds / 1'000'000.0f;
		last_time_ = current_time;
	}
}
