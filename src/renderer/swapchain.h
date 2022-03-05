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

		// Helper functions.
		VkSurfaceCapabilitiesKHR GetSurfaceCapabilities() const;

		VkSurfaceFormatKHR ChooseSurfaceFormat() const;

		VkPresentModeKHR ChooseSwapchainPresentMode() const;

		VkExtent2D GetSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

		VkSwapchainKHR swapchain_{};
		Context* context_{};
	};
}
