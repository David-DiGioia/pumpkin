#include "renderer_types.h"

namespace renderer
{
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
