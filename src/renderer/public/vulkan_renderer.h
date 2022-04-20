#pragma once

#include <array>
#include <vector>
#include "volk.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "context.h"
#include "swapchain.h"
#include "pipeline.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "vulkan_util.h"
#include "descriptor_set.h"

namespace renderer
{
	constexpr uint32_t FRAMES_IN_FLIGHT{ 2 };

	struct RenderObject
	{
		Mesh* mesh;
		VertexType vertex_type;

		struct UniformBuffer
		{
			glm::mat4 transform;
		} uniform_buffer;

		BufferResource ubo_buffer_resource;
		DescriptorSetResource ubo_descriptor_set_resource;
	};

	struct FrameResources
	{
		VkCommandBuffer command_buffer;
		VkFence render_done_fence;
		VkSemaphore image_acquired_semaphore;
		VkSemaphore render_done_semaphore;
	};

	class VulkanRenderer
	{
	public:
		void Initialize(GLFWwindow* window);

		void CleanUp();

		// Waits for the last frame with the same index, since the frame resources
		// will be occupied until it finishes rendering.
		//
		// Do all CPU work that mutates render objects between WaitForLastFrame() and Render().
		void WaitForLastFrame();

		// Render the list of render objects.
		// 
		// Note that the render objects are externally synchronized, meaning the caller must
		// mutate the render objects only corresponding to the current frame index and after
		// waiting for WaitForLastFrame().
		// 
		// Returns the index of the current frame-in-flight.
		uint32_t Render(const std::vector<RenderObject>* render_objects);

		void LoadMeshesGLTF(tinygltf::Model& model, std::vector<Mesh>* out_meshes);

		void InitializeDescriptorSetLayouts();

		// Create a render object with the buffer resource and descriptors already associated
		// with the render object data.
		RenderObject CreateRenderObject();

		void UploadRenderObjectBufferToDevice(RenderObject* render_object);

	private:
		void Draw(VkCommandBuffer cmd, uint32_t image_index);

		void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForRender(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index);

		void NextFrame();

		FrameResources& GetCurrentFrame();

		void InitializePipelines();

		void InitializeFrameResources();

		void InitializeCommandBuffers();

		void InitializeSyncObjects();

		Context context_{};
		Swapchain swapchain_{};
		GraphicsPipeline graphics_pipeline_{};
		VkCommandPool command_pool_{};
		Allocator allocator_{};
		DescriptorAllocator descriptor_allocator_{};
		VulkanUtil vulkan_util_{};

		DescriptorSetLayoutResource render_object_layout_resource_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};
	};
}
