#pragma once

#include <cstdint>

// Should not include any headers from Pumpkin, since most files include renderer_types.h.
#include "volk.h"
#include "glm/glm.hpp"

struct ImVec2;

namespace renderer
{
	// Handles.

	typedef uint64_t RenderObjectHandle; // Index into the render objects vector.

	// Constants.

	constexpr uint32_t FRAMES_IN_FLIGHT{ 2 };

	constexpr uint32_t VERTEX_BINDING{ 0 };
	constexpr uint32_t INSTANCE_BINDING{ 1 };

	// Types.

	enum class VertexAttributes
	{
		POSITION_NORMAL_COORD,
		POSITION_NORMAL,
		POSITION,
		MPM_PARTICLE,
		MPM_NODE,
		RIGID_BODY_VOXEL,
		NONE,
	};

	enum class MaterialProperty
	{
		COLOR,
		METALLIC,
		ROUGHNESS,
		EMISSION,
	};

	struct Extent
	{
		uint32_t width;
		uint32_t height;

		Extent();

		Extent(uint32_t w, uint32_t h);

		Extent(ImVec2 vec);

		const Extent& operator=(const Extent& other);

		bool operator==(const Extent& other);

		bool operator!=(const Extent& other);
	};
}
