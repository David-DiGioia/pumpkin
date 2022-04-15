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
		POSITION_NORMAL,
	};

	struct Vertex
	{
		glm::vec3 position;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct Triangle
	{
		Vertex v0;
		Vertex v1;
		Vertex v2;
	};

	struct Mesh
	{
		std::vector<Triangle> tris;
		BufferResource buffer_resource;
	};

	// Load all the meshes from a glTF file into out_meshes.
	void LoadMeshesGLTF(const tinygltf::Model& model, std::vector<Mesh>* out_meshes);

	Mesh LoadTriangle(const Context& context, Allocator& alloc);
}