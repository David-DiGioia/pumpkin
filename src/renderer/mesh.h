#pragma once

#include <vector>
#include "glm/glm.hpp"
#include "volk.h"

#include "memory_allocator.h"

namespace renderer
{
	constexpr uint32_t vertex_binding{ 0 };
	constexpr uint32_t instance_binding{ 1 };

	struct Vertex
	{
		glm::vec3 position;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};

	struct Triangle
	{
		Vertex v0;
		Vertex v1;
		Vertex v2;
	};

	struct Mesh
	{
		std::vector<Triangle> tris;
		BufferResource buffer_resource;
	};

	Mesh LoadTriangle(const Context& context, Allocator& alloc);
}