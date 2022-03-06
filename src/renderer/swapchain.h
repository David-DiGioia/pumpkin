#pragma once

#include <vector>

#include "volk.h"
#include "context.h"

namespace renderer
{
	class Swapchain
	{
	public:
		void Initialize(Context* context);

		void CleanUp();

	private:
		void InitializeSwapchain();

		void InitializeImageResources();

		// Helper functions.
		VkSurfaceCapabilitiesKHR GetSurfaceCapabilities() const;

		VkSurfaceFormatKHR ChooseSurfaceFormat() const;

		VkPresentModeKHR ChooseSwapchainPresentMode() const;

		VkExtent2D GetSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

		VkSwapchainKHR swapchain_{};
		Context* context_{};
		std::vector<VkImage> images_{};
		std::vector<VkImageView> image_views_{};

		uint32_t swapchain_image_count_{};
		VkFormat swapchain_image_format_{};
	};
}
