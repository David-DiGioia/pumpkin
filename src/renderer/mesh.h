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
		glm::vec3 tangent;
		glm::vec2 tex_coord;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes(VertexAttributes attributes);
	};

	struct Material
	{
		// Backup values to use if texture index is NULL_TEXTURE_INDEX.
		glm::vec4 color;
		float metallic;
		float roughness;
		float emission;

		// There is no corresponding texture for IOR.
		float ior;

		// Indices into textures[].
		uint32_t color_index;
		uint32_t metallic_index;
		uint32_t roughness_index;
		uint32_t emission_index;
		uint32_t normal_index;

		uint32_t padding[3];
	};

	struct Geometry
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		BufferResource vertices_resource;
		BufferResource indices_resource;
	};

	struct Mesh
	{
		AccelerationStructure blas;
		std::vector<Geometry> geometries;
	};

	struct Particle
	{
		glm::vec3 position;
		int geometry_index;
	};

	// Return hash of this mesh's vertices.
	//
	// out_mesh must already have Mesh::geometries resized to number of primitives in gltf mesh.
	uint64_t LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh);

	// Returns hash of this mesh's indices.
	//
	// out_mesh must already have Mesh::geometries resized to number of primitives in gltf mesh.
	uint64_t LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh);

	// Calculate the tangents of a loaded mesh.
	void CalculateTangents(Mesh* out_mesh);

	std::string NameMesh(const std::vector<Geometry>& geometries);
}