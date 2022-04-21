#pragma once

#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "volk.h"

// Disable warnings from tiny_gltf.
#define _CRT_SECURE_NO_WARNINGS
#include <codeanalysis\warnings.h>
#pragma warning( push )
#pragma warning ( disable : ALL_CODE_ANALYSIS_WARNINGS )
#include "tiny_gltf.h"
#pragma warning( pop )

#include "memory_allocator.h"

namespace renderer
{
	constexpr uint32_t vertex_binding{ 0 };
	constexpr uint32_t instance_binding{ 1 };

	enum class VertexType
	{
		NONE,
		POSITION_ONLY,
		POSITION_NORMAL_COORD,
	};

	struct Vertex
	{
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 tex_coord{};

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct Mesh
	{
		std::vector<Vertex> vertices{};
		std::vector<uint16_t> indices{};

		BufferResource vertices_resource{};
		BufferResource indices_resource{};
	};

	void LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<Vertex>* out_vertices);

	void LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<uint16_t>* out_indices);
}