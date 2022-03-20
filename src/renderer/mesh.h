#include <vector>
#include "glm/glm.hpp"
#include "volk.h"

namespace renderer
{
	struct Vertex
	{
		glm::vec3 position;

		static std::vector<VkVertexInputAttributeDescription> GetVertexAttributes();
	};
}