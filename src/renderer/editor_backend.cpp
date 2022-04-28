#include "editor_backend.h"

#include "volk.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "vulkan_renderer.h"
#include "vulkan_util.h"

namespace renderer
{
	void EditorBackend::Initialize(VulkanRenderer* renderer)
	{
		renderer_ = renderer;

		CreateDescriptorPool();
		InitializeImGui();

		// Call editor's custom initialization function.
		info_.initialization_callback(info_.user_data);
	}

	void EditorBackend::CleanUp()
	{
		vkDestroyDescriptorPool(renderer_->context_.device, descriptor_pool_, nullptr);
		ImGui_ImplVulkan_Shutdown();
	}

	void EditorBackend::RenderGui(VkCommandBuffer cmd)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Call editor's custom gui callback.
		info_.gui_callback((ImTextureID)render_target_, info_.user_data);

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	void EditorBackend::SetEditorInfo(const EditorInfo& editor_info)
	{
		info_ = editor_info;
	}

	void EditorBackend::CreateDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> pool_sizes{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
		};

		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 1000,
			.poolSizeCount = (uint32_t)pool_sizes.size(),
			.pPoolSizes = pool_sizes.data(),
		};

		VkResult result{ vkCreateDescriptorPool(renderer_->context_.device, &pool_info, nullptr, &descriptor_pool_) };
		CheckResult(result, "Failed to create imgui descriptor pool.");
	}

	void EditorBackend::InitializeImGui()
	{
		// This initializes the core structures of imgui
		ImGui::CreateContext();

		// This initializes imgui for GLFW.
		ImGui_ImplGlfw_InitForVulkan(renderer_->context_.window, true);

		// This initializes imgui for Vulkan.
		ImGui_ImplVulkan_InitInfo init_info{
			.Instance = renderer_->context_.instance,
			.PhysicalDevice = renderer_->context_.physical_device,
			.Device = renderer_->context_.device,
			.Queue = renderer_->context_.graphics_queue,
			.DescriptorPool = descriptor_pool_,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.UseDynamicRendering = true,
			.ColorAttachmentFormat = renderer_->swapchain_.GetImageFormat(),
		};

		// Pass null handle since we're using dynamic rendering.
		ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

		// Execute a gpu command to upload imgui font textures.
		renderer_->vulkan_util_.Begin();
		auto& cmd{ renderer_->vulkan_util_.GetCommandBuffer() };
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		renderer_->vulkan_util_.Submit();

		// Clear font textures from cpu data.
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}
