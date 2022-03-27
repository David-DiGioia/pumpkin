#pragma once

#include <array>
#include "volk.h"
#include "GLFW/glfw3.h"

#include "context.h"
#include "swapchain.h"
#include "pipeline.h"

namespace renderer
{
	constexpr uint32_t FRAMES_IN_FLIGHT{ 2 };

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

		void Render();

	private:
		void Draw(VkCommandBuffer cmd, uint32_t image_index);

		void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForRender(VkCommandBuffer cmd, uint32_t image_index);

		void TransitionSwapImageForPresent(VkCommandBuffer cmd, uint32_t image_index);

		void NextFrame();

		FrameResources& GetCurrentFrame();

		void InitializeFrameResources();

		void InitializeCommandBuffers();

		void InitializeSyncObjects();

		Context context_{};
		Swapchain swapchain_{};
		GraphicsPipeline graphics_pipeline_{};
		VkCommandPool command_pool_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_;
	};
}
