#include "swapchain.h"

#include <algorithm>
#include <limits>
#include "GLFW/glfw3.h"

#include "vulkan_util.h"

namespace renderer
{
	// Main functions ------------------------------------------------------------------------------------------

	void Swapchain::Initialize(Context* context)
	{
		context_ = context;
		InitializeSwapchain();
	}

	void Swapchain::InitializeSwapchain()
	{
		// Triple buffering.
		constexpr uint32_t swapchain_image_count{ 3 };

		VkSurfaceCapabilitiesKHR capabilities{ GetSurfaceCapabilities() };
		VkSurfaceFormatKHR format{ ChooseSurfaceFormat() };
		VkPresentModeKHR present_mode{ ChooseSwapchainPresentMode() };
		VkExtent2D extent{ GetSwapchainExtent(capabilities) };

		VkSwapchainCreateInfoKHR swapchain_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.flags = 0,
			.surface = context_->surface,
			.minImageCount = std::max(swapchain_image_count, capabilities.minImageCount),
			.imageFormat = format.format,
			.imageColorSpace = format.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0, // Only for sharing mode concurrent.
			.pQueueFamilyIndices = nullptr, // Only for sharing mode concurrent.
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE,
		};

		VkResult result = vkCreateSwapchainKHR(context_->device, &swapchain_info, nullptr, &swapchain_);
		CheckResult(result, "Failed to create swapchain.");
	}

	void Swapchain::CleanUp()
	{
		vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
	}




	// Helper functions ----------------------------------------------------------------------------------------

	VkSurfaceCapabilitiesKHR Swapchain::GetSurfaceCapabilities() const
	{
		VkSurfaceCapabilities2KHR capabilities{
			.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		};

		VkPhysicalDeviceSurfaceInfo2KHR surface_info{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			.surface = context_->surface,
		};

		vkGetPhysicalDeviceSurfaceCapabilities2KHR(context_->physical_device, &surface_info, &capabilities);

		return capabilities.surfaceCapabilities;
	}

	VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat() const
	{
		constexpr VkFormat desired_format{ VK_FORMAT_B8G8R8A8_SRGB };
		constexpr VkColorSpaceKHR desired_color_space{ VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

		VkPhysicalDeviceSurfaceInfo2KHR surface_info{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			.surface = context_->surface,
		};

		uint32_t format_count{};
		vkGetPhysicalDeviceSurfaceFormats2KHR(context_->physical_device, &surface_info, &format_count, nullptr);
		std::vector<VkSurfaceFormat2KHR> formats(format_count);
		for (auto& format : formats) {
			format.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		}
		vkGetPhysicalDeviceSurfaceFormats2KHR(context_->physical_device, &surface_info, &format_count, formats.data());

		for (const auto& format : formats)
		{
			bool found_desired_format{ format.surfaceFormat.format == desired_format };
			bool found_desired_color_space{ format.surfaceFormat.colorSpace == desired_color_space };

			if (found_desired_format && found_desired_color_space) {
				return format.surfaceFormat;
			}
		}

		// If we don't find desired format just return first one.
		return formats[0].surfaceFormat;
	}

	VkPresentModeKHR Swapchain::ChooseSwapchainPresentMode() const
	{
		constexpr VkPresentModeKHR desired_present_mode{ VK_PRESENT_MODE_MAILBOX_KHR };

		uint32_t present_mode_count{};
		vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physical_device, context_->surface, &present_mode_count, nullptr);
		std::vector<VkPresentModeKHR> present_modes(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physical_device, context_->surface, &present_mode_count, present_modes.data());

		for (auto present_mode : present_modes)
		{
			if (present_mode == desired_present_mode) {
				return present_mode;
			}
		}

		// If we can't find our desired present mode just return FIFO.
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D Swapchain::GetSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
	{
		// If currentExtent has max value then the surface size is determined by the extent of the swapchain.
		if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
		{
			// We can't just return the width_ and height_ variables because these are GLFW screen coordinates
			// which don't always correspond exactly to pixels. Vulkan needs the extents specified in pixels.
			int width{};
			int height{};
			glfwGetFramebufferSize(context_->window, &width, &height);

			VkExtent2D extent{ (uint32_t)width, (uint32_t)height };

			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return extent;
		} else
		{
			// Otherwise currentExtent is equal to the current extents of the surface
			// and we create the swapchain with the same size.
			return capabilities.currentExtent;
		}
	}

}
