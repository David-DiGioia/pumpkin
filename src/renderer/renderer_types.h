#pragma once

#include <cstdint>
#include <limits>

namespace renderer
{
	// Handles.

	typedef uint64_t RenderObjectHandle;

	constexpr uint64_t NULL_HANDLE{ std::numeric_limits<uint64_t>().max() };

	// Constants.

	constexpr uint32_t FRAMES_IN_FLIGHT{ 2 };

	constexpr uint32_t VERTEX_BINDING{ 0 };
	constexpr uint32_t INSTANCE_BINDING{ 1 };

	// Types.

	struct Extent
	{
		uint32_t width;
		uint32_t height;

		const Extent& operator=(const Extent& other);

		bool operator==(const Extent& other);

		bool operator!=(const Extent& other);
	};
}
