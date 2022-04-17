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
#include "mesh_types.h"

namespace renderer
{
	void LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<Vertex>* out_vertices);

	void LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<uint16_t>* out_indices);
}