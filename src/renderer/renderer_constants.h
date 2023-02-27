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
}
