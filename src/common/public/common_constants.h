#pragma once

#include "math_util.h"

constexpr float PI{ 3.1415926535f };

constexpr uint32_t NULL_INDEX{ std::numeric_limits<uint32_t>().max() };  // Index specifying null or invalid.

constexpr float GRID_SIZE{ 0.05f };             // Grid cell width.
constexpr float CHUNK_WIDTH{ 1.0f };            // Width of whole chunk.
constexpr uint32_t CHUNK_ROW_VOXEL_COUNT{ 64 }; // Total size of particle chunk dimension.
constexpr uint32_t PARTICLE_GROUP_COUNT{ 16 };  // Number of workgroups in each dimension.

constexpr float PARTICLE_WIDTH{ CHUNK_WIDTH / CHUNK_ROW_VOXEL_COUNT };
constexpr float PARTICLE_VOLUME{ PARTICLE_WIDTH * PARTICLE_WIDTH * PARTICLE_WIDTH };
constexpr uint32_t CHUNK_TOTAL_VOXEL_COUNT{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
constexpr uint32_t GRID_NODE_ROW_COUNT{ (uint32_t)constexpr_ceil(CHUNK_WIDTH / GRID_SIZE) };
constexpr uint32_t GRID_NODE_COUNT{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };
