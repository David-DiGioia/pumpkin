#pragma once

#include <filesystem>
#include <limits>

namespace renderer
{
	const std::filesystem::path SPIRV_PREFIX{ "../shaders/" };

	// Path for a shader to signify that it's unused, eg. for hit groups.
	const std::filesystem::path SHADER_UNUSED_PATH{ "" };

	constexpr uint64_t NULL_HANDLE{ std::numeric_limits<uint64_t>().max() }; // Handle specifying null or invalid.
	constexpr uint32_t NULL_INDEX{ std::numeric_limits<uint32_t>().max() };  // Index specifying null or invalid.
	constexpr uint32_t MAX_BINDLESS_TEXTURES{ 512 };                         // Maximum number of texture descriptors in the bindless array.

	// Flags to change renderer functionality.
	constexpr bool DYNAMIC_PARTICLE_MESH_CPU_BUILD{ true }; // Build dynamic particle meshes on CPU instead of GPU.
	constexpr bool DISABLE_STATIC_PARTICLE_MESH{ false };   // For debugging. Draws static particles as individual cubes, not a shell.
}
