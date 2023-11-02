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
		void DumpRenderData(
			nlohmann::json& j,
			const std::filesystem::path& vertex_path,
			const std::filesystem::path& index_path,
			const std::filesystem::path& texture_path);

		// The out_material_indices writes a list of material indices used by the newly loaded geometries, to update user count for materials.
		void LoadRenderData(
			nlohmann::json& j,
			const std::filesystem::path& vertex_path,
			const std::filesystem::path& index_path,
			const std::filesystem::path& texture_path,
			std::vector<int>* out_material_indices);

#ifdef EDITOR_ENABLED
		void ClearOutlineSets();

		void AddOutlineSet(const std::vector<renderer::RenderObjectHandle>& selection_set, const glm::vec4& color);

		void SetParticleOverlayEnabled(bool render_grid, bool render_nodes, bool rasterize_particles, bool use_particle_depth);

		void SetParticleOverlay(renderer::RenderObjectHandle render_object);

		void SetParticleColorMode(uint32_t color_mode);

		void SetNodeColorMode(uint32_t color_mode);

		void SetParticleColorModeMaxValue(float max_value);

		void SetNodeColorModeMaxValue(float max_value);

		void SetRenderCubeNodesEnabled(bool enabled);
#endif

		void QueueRaycast(const glm::vec3& origin, const glm::vec3& direction);

		std::vector<pmk::Rayhit> CastQueuedRays();

		Node* GetNodeByRenderObject(renderer::RenderObjectHandle handle);

		void AddRenderObjectToNode(Node* node, renderer::RenderObjectHandle handle);

		void SetRenderObjectVisible(renderer::RenderObjectHandle render_object_handle, bool visible);

		uint32_t CreateTexture(unsigned char* data, uint32_t width, uint32_t height, uint32_t channels, bool color_data);

		uint32_t GetTextureCount() const;

		void ImportShader(const std::filesystem::path& spirv_path);

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
		float physics_time_accumulator_{};
		std::chrono::steady_clock::time_point last_time_{};

		std::vector<renderer::Raycast> queued_raycasts_{};

		uint32_t width_{ 1920 };
		uint32_t height_{ 1080 };
	};
}
