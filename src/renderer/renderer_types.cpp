#include "renderer_types.h"

#include "imgui.h"

namespace renderer
{
	Extent::Extent()
		: width{}
		, height{}
	{
	}

	Extent::Extent(uint32_t w, uint32_t h)
		: width{ w }
		, height{ h }
	{
	}

	Extent::Extent(ImVec2 vec)
		: width{ (uint32_t)vec.x }
		, height{ (uint32_t)vec.y }
	{
	}

	const Extent& Extent::operator=(const Extent& other)
	{
		width = other.width;
		height = other.height;
		return other;
	}

	bool Extent::operator==(const Extent& other)
	{
		return (width == other.width) && (height == other.height);
	}

	bool Extent::operator!=(const Extent& other)
	{
		return !operator==(other);
	}
}
