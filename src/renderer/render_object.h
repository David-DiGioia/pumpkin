#pragma once

#include "memory_allocator.h"
#include "descriptor_set.h"

namespace renderer
{
	struct RenderObject
	{
		uint32_t mesh_idx;

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
