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

#include "descriptor_set.h"
#include "memory_allocator.h"

namespace renderer
{
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

	struct RenderObject
	{
		// TODO: Later handle multiple primitives per mesh from GLTF file.
		//       This occurs when a single mesh has multiple materials.
		//       For raytracing we probably want to implement with geometry indexing.
		Mesh* mesh;
		VertexType vertex_type;

		struct UniformBuffer
		{
			glm::mat4 transform;
		} uniform_buffer;

		BufferResource ubo_buffer_resource;
		DescriptorSetResource ubo_descriptor_set_resource;
	};

	void LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<Vertex>* out_vertices);

	void LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<uint16_t>* out_indices);
}