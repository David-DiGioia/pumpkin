#pragma once

#include <cstdint>
#include "glm/glm.hpp"

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

constexpr int32_t constexpr_ceil(float num)
{
    return (static_cast<float>(static_cast<int32_t>(num)) == num)
        ? static_cast<int32_t>(num)
        : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

void SingularValueDecomposition(const glm::mat3& mat, glm::mat3* u, glm::mat3* s, glm::mat3* v);

void PolarDecomposition(const glm::mat3& mat, glm::mat3* r, glm::mat3* s);
