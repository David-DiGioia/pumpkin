#pragma once

#include <new>
#include "math_util.h"

#if defined(__cpp_lib_hardware_interference_size)
// Default cacheline size from runtime.
constexpr size_t CL_SIZE = std::hardware_constructive_interference_size;
#else
// Most common cacheline size otherwise.
constexpr size_t CL_SIZE = 64;
#endif

constexpr float PI{ 3.1415926535f };

constexpr uint32_t NULL_INDEX{ std::numeric_limits<uint32_t>().max() };  // Index specifying null or invalid.

constexpr float GRID_SIZE{ 0.05f };             // Grid cell width.
constexpr float CHUNK_WIDTH{ 8.0f };            // Width of whole chunk.
constexpr uint32_t CHUNK_ROW_VOXEL_COUNT{ 64 }; // Total size of particle chunk dimension.
constexpr uint32_t PARTICLE_GROUP_COUNT{ 16 };  // Number of workgroups in each dimension.

constexpr float PARTICLE_WIDTH{ CHUNK_WIDTH / CHUNK_ROW_VOXEL_COUNT };
constexpr float PARTICLE_WIDTH_SQUARED{ PARTICLE_WIDTH * PARTICLE_WIDTH };
constexpr float PARTICLE_RADIUS{ PARTICLE_WIDTH * 0.5f };
constexpr float PARTICLE_VOLUME{ PARTICLE_WIDTH * PARTICLE_WIDTH * PARTICLE_WIDTH };
constexpr uint32_t CHUNK_TOTAL_VOXEL_COUNT{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
constexpr uint32_t MAXIMUM_BLOCKS_IN_KERNEL{ 27 }; // Since 3^3 = 27. Assuming kernel radius is less or equal to grid spacing.

