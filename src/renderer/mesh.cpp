#include "mesh.h"

#include "logger.h"

namespace renderer
{
	std::vector<VkVertexInputAttributeDescription> Vertex::GetVertexAttributes()
	{
		size_t size_check{ 0 }; // Make sure we don't forget any members of Vertex.
		std::vector<VkVertexInputAttributeDescription> attributes{};

		// Position.
		size_check += sizeof(position);
		attributes.push_back({
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT, // vec3
				.offset = offsetof(Vertex, position),
			});

		if (size_check != sizeof(Vertex)) {
			logger::Error("Vertex attributes do not match vertex struct.\
				Did you remember to update Vertex::GetVertexAttributes after adding new members?\n");
		}

		return attributes;
	}
}
