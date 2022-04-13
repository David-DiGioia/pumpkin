#include "mesh.h"

#include <type_traits>

#include "logger.h"
#include "memory_allocator.h"

#define VERTEX_ATTRIBUTE(loc, attr)								\
	VkVertexInputAttributeDescription{							\
		.location = loc,										\
		.binding = vertex_binding,								\
		.format = GetVulkanFormat<decltype(Vertex::attr)>(),	\
		.offset = offsetof(Vertex, attr),						\
	}															\

template <typename T>
VkFormat GetVulkanFormat()
{
	if (std::is_same<glm::vec4, T>::value) {
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	}
	else if (std::is_same<glm::vec3, T>::value) {
		return VK_FORMAT_R32G32B32_SFLOAT;
	}
	else if (std::is_same<glm::vec2, T>::value) {
		return VK_FORMAT_R32G32_SFLOAT;
	}
	else if (std::is_same<float, T>::value) {
		return VK_FORMAT_R32_SFLOAT;
	}
	else if (std::is_same<glm::ivec4, T>::value) {
		return VK_FORMAT_R32G32B32A32_SINT;
	}
	else if (std::is_same<glm::ivec3, T>::value) {
		return VK_FORMAT_R32G32B32_SINT;
	}
	else if (std::is_same<glm::ivec2, T>::value) {
		return VK_FORMAT_R32G32_SINT;
	}
	else if (std::is_same<int, T>::value) {
		return VK_FORMAT_R32_SINT;
	}
	else if (std::is_same<glm::uvec4, T>::value) {
		return VK_FORMAT_R32G32B32A32_UINT;
	}
	else if (std::is_same<glm::uvec3, T>::value) {
		return VK_FORMAT_R32G32B32_UINT;
	}
	else if (std::is_same<glm::uvec2, T>::value) {
		return VK_FORMAT_R32G32_UINT;
	}
	else if (std::is_same<uint32_t, T>::value) {
		return VK_FORMAT_R32_UINT;
	}

	logger::Error("Unknown Vulkan format requested.");

	return VK_FORMAT_UNDEFINED;
}


namespace renderer
{
	std::vector<VkVertexInputAttributeDescription> Vertex::GetVertexAttributes()
	{
		return {
			VERTEX_ATTRIBUTE(0, position),
		};
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
