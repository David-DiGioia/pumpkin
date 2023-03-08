#pragma once

#include <chrono>
#include <filesystem>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "nlohmann/json.hpp"

#include "vulkan_renderer.h"
#include "scene.h"

namespace pmk
{
	struct Rayhit
	{
		Node* node;
		glm::vec3 position;
	};

	class Pumpkin
	{
	public:
		void Initialize();

		void Start();

		void MainLoop();

		void CleanUp();

		void SetImGuiCallbacksInfo(const renderer::ImGuiCallbacks& editor_info);

		void SetEditorViewportSize(const renderer::Extent& extent);

		void SetCameraPosition(const glm::vec3& pos);

		Scene& GetScene();

		renderer::Mesh* GetMesh(renderer::RenderObjectHandle render_object);

		const std::vector<int>& GetMaterialIndices(renderer::RenderObjectHandle render_object);

		std::vector<renderer::Material*>& GetMaterials();

		void UpdateMaterials();

		renderer::Material* MakeMaterialUnique(uint32_t material_index);

		void SetMaterialIndex(renderer::RenderObjectHandle render_object, uint32_t geometry_index, int material_index);

		void UpdateObjectBuffers();

		float GetDeltaTime() const;

		// Write render data info to json, and vertex data to a binary file.
		void DumpRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path) const;

		// The out_material_indices writes a list of material indices used by the newly loaded geometries, to update user count for materials.
		void LoadRenderData(nlohmann::json& j, const std::filesystem::path& vertex_path, const std::filesystem::path& index_path, std::vector<int>* out_material_indices);

		void ClearOutlineSets();

		void AddOutlineSet(const std::vector<renderer::RenderObjectHandle>& selection_set, const glm::vec4& color);

		void QueueRaycast(const glm::vec3& origin, const glm::vec3& direction);

		std::vector<pmk::Rayhit> CastQueuedRays();

		Node* GetNodeByRenderObject(renderer::RenderObjectHandle handle);

		void AddRenderObjectToNode(Node* node, renderer::RenderObjectHandle handle);

		uint32_t CreateTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels);

	private:
		// General work the host needs to do each frame.
		void HostWork();

		// Work the host needs to do that modifies the render objects.
		void HostRenderWork();

		void UpdateDeltaTime();

		GLFWwindow* window_{};
		renderer::VulkanRenderer renderer_{};
		Scene scene_{};

		float delta_time_{};
		std::chrono::steady_clock::time_point last_time_{};

		std::vector<renderer::Raycast> queued_raycasts_{};

		uint32_t width_{ 1920 };
		uint32_t height_{ 1080 };
	};
}
