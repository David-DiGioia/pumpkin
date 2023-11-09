#pragma once

#include <filesystem>
#include <limits>
#include <cmath>

namespace renderer
{
	const std::filesystem::path SPIRV_PREFIX{ "../shaders/" };

	// Path for a shader to signify that it's unused, eg. for hit groups.
	const std::filesystem::path SHADER_UNUSED_PATH{ "" };

	constexpr uint64_t NULL_HANDLE{ std::numeric_limits<uint64_t>().max() }; // Handle specifying null or invalid.
	constexpr uint32_t NULL_INDEX{ std::numeric_limits<uint32_t>().max() };  // Index specifying null or invalid.
	constexpr uint32_t MAX_BINDLESS_TEXTURES{ 512 };                         // Maximum number of texture descriptors in the bindless array.

	constexpr float GRID_SIZE{ 0.04f };             // Grid cell width.
	constexpr float CHUNK_WIDTH{ 1.0f };            // Width of whole chunk.
	constexpr uint32_t CHUNK_ROW_VOXEL_COUNT{ 64 }; // Total size of particle chunk dimension.
	constexpr uint32_t PARTICLE_GROUP_COUNT{ 16 };  // Number of workgroups in each dimension.

	constexpr float PARTICLE_WIDTH{ CHUNK_WIDTH / CHUNK_ROW_VOXEL_COUNT };
	constexpr uint32_t CHUNK_TOTAL_VOXEL_COUNT{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
	const uint32_t GRID_NODE_ROW_COUNT{ (uint32_t)std::ceil(CHUNK_WIDTH / GRID_SIZE) };
	const uint32_t GRID_NODE_COUNT{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };

	enum class MPMInterpolationKernel
	{
		LINEAR,
		QUADRATIC,
		CUBIC,
	};

	// Flags to change renderer functionality.
	constexpr bool DYNAMIC_PARTICLE_MESH_CPU_BUILD{ true }; // Build dynamic particle meshes on CPU instead of GPU.
	constexpr bool DISABLE_STATIC_PARTICLE_MESH{ true };    // For debugging. Draws static particles as individual cubes, not a shell.
	constexpr MPMInterpolationKernel MPM_INTERPOLATION_KERNEL{ MPMInterpolationKernel::QUADRATIC };
}
