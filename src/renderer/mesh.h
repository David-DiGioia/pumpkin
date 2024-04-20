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
	struct Vertex
	{
		glm::vec4 position;
		glm::vec4 normal;
		glm::vec4 tangent;
		glm::vec2 tex_coord;
		float padding[2];

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes(VertexAttributes attributes);
	};

#ifdef EDITOR_ENABLED
	struct MPMDebugParticleInstance
	{
		float mass;
		float mu;
		float lambda;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 gradient;
		// Deformation is a 3x3 matrix, but we need to use 3 separate vertex attribute bindings.
		glm::vec3 elastic_col_0;
		glm::vec3 elastic_col_1;
		glm::vec3 elastic_col_2;
		glm::vec3 plastic_col_0;
		glm::vec3 plastic_col_1;
		glm::vec3 plastic_col_2;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct MPMDebugNodeInstance
	{
		float mass;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 momentum;
		glm::vec3 force;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct RigidBodyDebugVoxelInstance
	{
		glm::vec3 position;
		glm::vec3 normal;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};
#endif

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

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR acceleration_structure;
		BufferResource buffer_resource;
	};

	struct Mesh
	{
		AccelerationStructure blas;
		std::vector<Geometry> geometries;

		// If true, destroying the mesh will not destroy its vertex/index device buffers.
		// Needed when multiple meshes share the same buffers.
		bool preserve_geometry_buffers;

		// If true, then a single renderer::Geometry must be in renderer::Mesh that's passed to QueueBlas().
		// Then multiple Vulkan geometries will be constructed refercing the vertex/index buffer from the renderer::Geometry
		// and using the offsets provided in build_ranges.
		// If false, then each renderer::Geometry will correspond to a Vulkan geometry like normal, and build range offsets can be 0.
		bool use_single_buffer;

		// If true, then this mesh data will be written to disk when the project is saved.
		// This would be false for generated mesh data, like voxels..
		bool write_to_disk;
		std::vector<uint32_t> index_byte_offsets;  // Only used if use_single_buffer is true.
	};

	// Extra info about a mesh needed for building a BLAS.
	// This is needed when mesh is built on GPU and this info can't be caluclated
	// from Geometry::vertices and Geometry::indices since they are empty.
	struct MeshBlasInfo
	{
		std::vector<uint32_t> max_indices;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges; // Each build range corresponds to one geometry.
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