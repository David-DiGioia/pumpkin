#pragma once

#include <string>
#include "glm/glm.hpp"
#include "volk.h"

#include "logger.h"

#define VERTEX_ATTRIBUTE(loc, attr)								\
	VkVertexInputAttributeDescription{							\
		.location = loc,										\
		.binding = vertex_binding,								\
		.format = GetVulkanFormat<decltype(Vertex::attr)>(),	\
		.offset = offsetof(Vertex, attr),						\
	}

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

void CheckResult(VkResult result, const std::string& msg);
