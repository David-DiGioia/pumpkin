#pragma once

#include <filesystem>

const std::filesystem::path SPIRV_PREFIX{ "../shaders/" };

// Path for a shader to signify that it's unused, eg. for hit groups.
const std::filesystem::path SHADER_UNUSED_PATH{ "" };
