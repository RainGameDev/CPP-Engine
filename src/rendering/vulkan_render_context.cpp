#include "rendering/vulkan_render_context.h"
#include "assets/material_loader.h"
#include "assets/obj_loader.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

void VulkanRenderingContext::init(GLFWwindow *window) {
  this->window = window;

  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  assetManager.addLoader(std::make_unique<ShaderLoader>(device));
  assetManager.loadDirectory("assets/shaders");
  createSwapChain();
  createImageViews();
  createDepthResources();
  createCommandPool();
  createDescriptorSetLayout();
  assetManager.addLoader(std::make_unique<TextureLoader>(
      device, physicalDevice, commandPool, queue, queueIndex));
  assetManager.loadDirectory("assets/textures");

  createDescriptorPool();
  assetManager.addLoader(std::make_unique<MaterialLoader>(
      device, physicalDevice, materialSetLayout, descriptorPool,
      commandPool, queue, assetManager));
  assetManager.loadDirectory("assets/materials");

  createUniformBuffers();
  createDescriptorSets();
  createGraphicsPipelineLayout();

  auto *materials = assetManager.getLoader<MaterialAsset>();
  auto mesh = loadObjMesh("assets/models/mesh.obj",
                          &materials->getAsset("brick.mat"));
  addMesh(std::move(mesh));
  createCommandBuffer();
  createSyncObjects();

  initImGui();
}

void VulkanRenderingContext::cleanup() {
  cleanupImGui();
}

void VulkanRenderingContext::updateUniformBuffer(uint32_t frame,
                                                  const UniformBufferObject &ubo) {
  lastUbo = ubo;
  memcpy(uniformBuffers[frame].mapped, &ubo, sizeof(ubo));
}

void VulkanRenderingContext::updateUniformBuffer(uint32_t frame) {
  memcpy(uniformBuffers[frame].mapped, &lastUbo, sizeof(lastUbo));
}

void VulkanRenderingContext::addMesh(Mesh mesh) {
  meshes.push_back(std::move(mesh));
}

uint32_t VulkanRenderingContext::findMemoryType(uint32_t typeFilter,
                                                 vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    bool typeSupported = typeFilter & (1 << i);
    bool propertiesSupported =
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

    if (typeSupported && propertiesSupported) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format VulkanRenderingContext::findSupportedDepthFormat() {
  std::vector<vk::Format> candidates = {
      vk::Format::eD32Sfloat,
      vk::Format::eD32SfloatS8Uint,
      vk::Format::eD24UnormS8Uint};
  for (auto format : candidates) {
    auto props = physicalDevice.getFormatProperties(format);
    if (props.optimalTilingFeatures &
        vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
      return format;
    }
  }
  throw std::runtime_error("failed to find supported depth format!");
}

void VulkanRenderingContext::createDepthResources() {
  depthFormat = findSupportedDepthFormat();

  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = depthFormat,
      .extent = {swapChainExtent.width, swapChainExtent.height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};
  depthImage = vk::raii::Image(device, imageInfo);

  auto memReqs = depthImage.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReqs.size,
      .memoryTypeIndex = findMemoryType(
          memReqs.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eDeviceLocal)};
  depthImageMemory = vk::raii::DeviceMemory(device, allocInfo);
  depthImage.bindMemory(*depthImageMemory, 0);

  vk::ImageViewCreateInfo viewInfo{
      .image = *depthImage,
      .viewType = vk::ImageViewType::e2D,
      .format = depthFormat,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  depthImageView = vk::raii::ImageView(device, viewInfo);
}

void VulkanRenderingContext::createDescriptorPool() {
  constexpr uint32_t maxMaterials = 64;

  std::array<vk::DescriptorPoolSize, 2> poolSizes = {
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                             maxConcurrentFrames},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                             maxMaterials * 4}};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames + maxMaterials,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
