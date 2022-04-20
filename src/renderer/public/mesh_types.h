#pragma once

#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "volk.h"

#include "memory_allocator.h"

namespace renderer
{
	constexpr uint32_t vertex_binding{ 0 };
	constexpr uint32_t instance_binding{ 1 };

	enum class VertexType
	{
		NONE,
		POSITION_ONLY,
		POSITION_NORMAL_COORD,
	};

	struct Vertex
	{
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 tex_coord{};

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct Mesh
	{
		std::vector<Vertex> vertices{};
		std::vector<uint16_t> indices{};

		BufferResource vertices_resource{};
		BufferResource indices_resource{};
	};
}