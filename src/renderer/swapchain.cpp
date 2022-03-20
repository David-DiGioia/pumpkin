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
		InitializeImageResources();
	}

	void Swapchain::InitializeSwapchain()
	{
		// Triple buffering.
		constexpr uint32_t swapchain_image_count{ 3 };

		VkSurfaceCapabilitiesKHR capabilities{ GetSurfaceCapabilities() };
		VkSurfaceFormatKHR format{ ChooseSurfaceFormat() };
		VkPresentModeKHR present_mode{ ChooseSwapchainPresentMode() };
		VkExtent2D extent{ GetSwapchainExtent(capabilities) };

		swapchain_image_format_ = format.format;

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

		VkResult result{ vkCreateSwapchainKHR(context_->device, &swapchain_info, nullptr, &swapchain_) };
		CheckResult(result, "Failed to create swapchain.");
	}

	void Swapchain::InitializeImageResources()
	{
		vkGetSwapchainImagesKHR(context_->device, swapchain_, &swapchain_image_count_, nullptr);
		images_.resize(swapchain_image_count_);
		vkGetSwapchainImagesKHR(context_->device, swapchain_, &swapchain_image_count_, images_.data());

		image_views_.resize(swapchain_image_count_);

		VkImageViewCreateInfo image_view_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.flags = 0,
			.image = VK_NULL_HANDLE, // We assign the image in the loop.
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain_image_format_,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		for (uint32_t i{ 0 }; i < swapchain_image_count_; ++i)
		{
			image_view_info.image = images_[i];

			VkResult result{ vkCreateImageView(context_->device, &image_view_info, nullptr, &image_views_[i]) };
			CheckResult(result, "Failed to create image view.");
		}
	}

	void Swapchain::CleanUp()
	{
		for (auto& view : image_views_) {
			vkDestroyImageView(context_->device, view, nullptr);
		}

		vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
	}

	VkFormat Swapchain::GetImageFormat() const
	{
		return swapchain_image_format_;
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
			Extents extents{ context_->GetWindowExtents() };

			VkExtent2D extent{ extents.width, extents.height };

			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return extent;
		}
		else
		{
			// Otherwise currentExtent is equal to the current extents of the surface
			// and we create the swapchain with the same size.
			return capabilities.currentExtent;
		}
	}

}
