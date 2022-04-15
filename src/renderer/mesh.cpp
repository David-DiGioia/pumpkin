#include "mesh.h"

#include <type_traits>
#include <vector>

#include "tiny_gltf.h"
#include "logger.h"
#include "memory_allocator.h"
#include "vulkan_util.h"

namespace renderer
{
	std::vector<VkVertexInputAttributeDescription> Vertex::GetVertexAttributes()
	{
		return {
			VERTEX_ATTRIBUTE(0, position),
		};
	}

	void LoadMeshesGLTF(const tinygltf::Model& model, std::vector<Mesh>* out_meshes)
	{
		// TODO!
	}

	Mesh LoadTriangle(const Context& context, Allocator& alloc)
	{
		Triangle tri{
			{{-1.0f, 0.0f, 0.0f}},
			{{1.0f, 0.0f, 0.0f}},
			{{0.0f, 1.0f, 0.0f}},
		};

		BufferResource resource{ alloc.CreateBufferResource(sizeof(Triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) };

		void* data{};
		vkMapMemory(context.device, *resource.memory, resource.offset, resource.size, 0, &data);
		memcpy(data, &tri, resource.size);
		vkUnmapMemory(context.device, *resource.memory);

		Mesh mesh{
			.tris = {tri},
			.buffer_resource = resource,
		};

		return mesh;
	}
}
