#pragma once

#include "memory_allocator.h"
#include "descriptor_set.h"

namespace renderer
{
	struct RenderObject
	{
		uint32_t mesh_idx;
		std::vector<int> material_indices; // The ith index is the material index for the ith geometry. Store it here to decouple material and mesh.

		// TODO: Maybe put all the render object transforms into a single buffer and store index here.
		//       Instead of having lots of tiny buffers.
		struct UniformBuffer
		{
			glm::mat4 transform;
		} uniform_buffer;

		BufferResource ubo_buffer_resource;
		DescriptorSetResource ubo_descriptor_set_resource;
	};
}
