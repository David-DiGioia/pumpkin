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
#include "ray_tracing.h"

namespace renderer
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct Material
	{
		glm::vec4 color;
		float metallic;
		float roughness;
		float ior;
		float emission;
	};

	struct Geometry
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		//int material_index;

		BufferResource vertices_resource;
		BufferResource indices_resource;
	};

	struct Mesh
	{
		AccelerationStructure blas;
		std::vector<Geometry> geometries;
	};

	// Return hash of this mesh's vertices.
	//
	// out_mesh must already have Mesh::geometries resized to number of primitives in gltf mesh.
	uint64_t LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh);

	// Returns hash of this mesh's indices.
	//
	// out_mesh must already have Mesh::geometries resized to number of primitives in gltf mesh.
	uint64_t LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh);

	std::string NameMesh(const std::vector<Geometry>& geometries);
}