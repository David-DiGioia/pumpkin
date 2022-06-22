#pragma once

template<typename V1, typename V2>
V1 CastVec2(const V2& vec)
{
	return V1{ vec.x, vec.y };
}

template<typename V1, typename V2>
V1 CastVec3(const V2& vec)
{
	return V1{ vec.x, vec.y, vec.z };
}