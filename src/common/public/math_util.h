#pragma once

#include <cstdint>
#include <vector>
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

// Group the elements T of a vector such that all elements with member 'key' equal to
// each other will be contiguous. Similar to function GroupByPhysicsIndex() but using key to group.
// We use two implementations to avoid overhead of std::function returning the relevant member.
template<typename T, uint32_t MAX_KEY>
void GroupByKey(std::vector<T>& v)
{
	// Use uint32_t type since this will later hold offset into tmp_v so must be as high as max particle count.
	std::vector<uint32_t> counts(MAX_KEY, 0);

	// Count occurrence of each category.
	for (const T& t : v) {
		++counts[t.key];
	}

	// Convert the counts into base offsets into tmp_v.
	uint32_t base_offset{ 0 };
	for (auto& count : counts)
	{
		if (count == 0) {
			continue;
		}

		uint32_t count_copy = count;
		count = base_offset;
		base_offset += count_copy;
	}

	// Write each category starting at their base offset into tmp_v.
	std::vector<T> tmp_v{};
	tmp_v.resize(v.size());
	for (const T& t : v) {
		tmp_v[counts[t.key]++] = t;
	}

	v = std::move(tmp_v);
}

// Curently unused because for some reason sorting is faster than this one.
template<typename T>
void GroupByPhysicsIndex(std::vector<T>& v)
{
	// Use uint32_t type since this will later hold offset into tmp_v so must be as high as max particle count.
	std::vector<uint32_t> counts(std::numeric_limits<uint8_t>::max() + 1, 0);

	{
		//ZoneScopedN("Count");
		// Count occurrence of each category.
		for (const T& t : v) {
			++counts[t.physics_material_index];
		}
	}

	{
		//ZoneScopedN("Convert to offsets");
		// Convert the counts into base offsets into tmp_v.
		uint32_t base_offset{ 0 };
		for (auto& count : counts)
		{
			if (count == 0) {
				continue;
			}

			uint32_t count_copy = count;
			count = base_offset;
			base_offset += count_copy;
		}
	}

	std::vector<T> tmp_v{};
	{
		//ZoneScopedN("Write to tmp_v");
		// Write each category starting at their base offset into tmp_v.
		tmp_v.resize(v.size());
		for (const T& t : v)
		{
			T& tmp_t{ tmp_v[counts[t.physics_material_index]++] };
			tmp_t.position = t.position;
			tmp_t.physics_material_index = t.physics_material_index;
		}
	}

	{
		//ZoneScopedN("Write to tmp_v");
		v = std::move(tmp_v);
	}
}

constexpr int32_t constexpr_ceil(float num)
{
    return (static_cast<float>(static_cast<int32_t>(num)) == num)
        ? static_cast<int32_t>(num)
        : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

void SingularValueDecomposition(const glm::mat3& mat, glm::mat3* u, glm::mat3* s, glm::mat3* v);

void PolarDecomposition(const glm::mat3& mat, glm::mat3* r, glm::mat3* s);
