#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_application.h"

void VulkanApplication::initImGui() {
  // Create its descripotrs.
  vk::DescriptorPoolSize poolSize{
      .type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1};
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize};
  imguiPool = device.createDescriptorPool(poolInfo);

  // Create imgui context
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForVulkan(window, true);

  // Create imgui vulkan
  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.Instance = *instance;
  initInfo.PhysicalDevice = *physicalDevice;
  initInfo.Device = *device;
  initInfo.QueueFamily = queueIndex;
  initInfo.Queue = *queue;
  initInfo.DescriptorPool = *imguiPool;
  initInfo.MinImageCount = 2;
  initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
  initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  initInfo.UseDynamicRendering = true;

  VkFormat colorFormat = static_cast<VkFormat>(swapChainSurfaceFormat.format);

  VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
  pipelineRenderingInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingInfo.colorAttachmentCount = 1;
  pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

  initInfo.UseDynamicRendering = true;
  initInfo.PipelineRenderingCreateInfo = pipelineRenderingInfo;

  ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanApplication::cleanupImGui() {
  device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
