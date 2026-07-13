#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_render_context.h"

void VulkanRenderingContext::initImGui() {
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForVulkan(window, true);

  VkFormat colorFormat = static_cast<VkFormat>(swapChainSurfaceFormat.format);

  VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
  pipelineRenderingInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingInfo.colorAttachmentCount = 1;
  pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.Instance = *instance;
  initInfo.PhysicalDevice = *physicalDevice;
  initInfo.Device = *device;
  initInfo.QueueFamily = queueIndex;
  initInfo.Queue = *queue;
  initInfo.DescriptorPoolSize = 512;
  initInfo.MinImageCount = 2;
  initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
  initInfo.UseDynamicRendering = true;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

  ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanRenderingContext::cleanupImGui() {
  device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
