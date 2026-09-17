#include "rendering/vulkan_render_context.h"
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/obj_loader.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "ecs/components.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "imgui_impl_vulkan.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

void VulkanRenderingContext::init(GLFWwindow *window, World &ecsWorld) {
  this->window = window;
  this->world = &ecsWorld;

  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createDepthResources();
  createCommandPool();
  createDescriptorSetLayout();
  createDescriptorPool();
  createUniformBuffers();
  createDescriptorSets();
  createDefaultMaterial();
  createGraphicsPipelineLayout();

  createCommandBuffer();
  createSyncObjects();

  initImGui();

  createViewportResources(viewportExtent.width, viewportExtent.height);
}

void VulkanRenderingContext::cleanup() {
  device.waitIdle();
  cleanupViewportResources();
  cleanupImGui();
}

void VulkanRenderingContext::updateUniformBuffer(
    uint32_t frame, const UniformBufferObject &ubo) {
  lastUbo = ubo;
  memcpy(uniformBuffers[frame].mapped, &ubo, sizeof(ubo));
}

void VulkanRenderingContext::updateUniformBuffer(uint32_t frame) {
  memcpy(uniformBuffers[frame].mapped, &lastUbo, sizeof(lastUbo));
}

void VulkanRenderingContext::ensureTransformBuffer(uint32_t frame,
                                                   uint32_t meshCount) {
  vk::DeviceSize required = transformStride * meshCount;
  if (required <= transformBufferSizes[frame])
    return;

  device.waitIdle();

  transformBuffers[frame].buffer = nullptr;
  transformBuffers[frame].memory = nullptr;

  transformBuffers[frame].buffer = vk::raii::Buffer(
      device, {.size = required,
               .usage = vk::BufferUsageFlagBits::eUniformBuffer,
               .sharingMode = vk::SharingMode::eExclusive});

  auto memReqs = transformBuffers[frame].buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReqs.size,
      .memoryTypeIndex =
          findMemoryType(memReqs.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent)};

  transformBuffers[frame].memory = vk::raii::DeviceMemory(device, allocInfo);
  transformBuffers[frame].buffer.bindMemory(*transformBuffers[frame].memory, 0);
  transformBuffers[frame].mapped =
      transformBuffers[frame].memory.mapMemory(0, required);
  transformBufferSizes[frame] = required;

  vk::DescriptorBufferInfo bufferInfo{.buffer = *transformBuffers[frame].buffer,
                                      .offset = 0,
                                      .range = transformStride};

  vk::WriteDescriptorSet write{.dstSet = descriptorSets[frame],
                               .dstBinding = 1,
                               .dstArrayElement = 0,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eUniformBufferDynamic,
                               .pBufferInfo = &bufferInfo};

  device.updateDescriptorSets(write, nullptr);
}

void VulkanRenderingContext::updateLightBuffer(uint32_t frame) {
  LightsUBO lightsubo{};
  lightsubo.count = 0;

  Query<LightComponent, TransformComponent> lightQuery(*world);
  lightQuery.for_each(
      [&](EntityId id, LightComponent &lc, TransformComponent &tc) {
        if (lightsubo.count >= MAX_LIGHTS)
          return;
        auto &l = lightsubo.lights[lightsubo.count];
        glm::vec3 forward = tc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

        if (std::holds_alternative<Directional>(lc.lightType)) {
          l.positionOrDirection = glm::vec4(glm::normalize(tc.position), 0.0f);
          l.direction = glm::vec4(forward, 0.0f);
        } else if (std::holds_alternative<Point>(lc.lightType)) {
          l.positionOrDirection = glm::vec4(tc.position, 1.0f);
          l.params.x = std::get<Point>(lc.lightType).radius;
          l.direction = glm::vec4(forward, 1.0f);
        } else if (std::holds_alternative<Spot>(lc.lightType)) {
          l.positionOrDirection = glm::vec4(tc.position, 2.0f);
          l.direction = glm::vec4(forward, 1.0f);
          auto &spot = std::get<Spot>(lc.lightType);
          l.params.x = spot.angle;
          l.params.y = spot.length;
        }
        l.colorAndIntensity = glm::vec4(lc.color, lc.intensity);
        lightsubo.count++;
      });

  memcpy(lightBuffers[frame].mapped, &lightsubo, sizeof(lightsubo));
}

uint32_t
VulkanRenderingContext::findMemoryType(uint32_t typeFilter,
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
  std::vector<vk::Format> candidates = {vk::Format::eD32Sfloat,
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
          memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
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

void VulkanRenderingContext::createViewportResources(uint32_t width,
                                                     uint32_t height) {
  if (width == 0 || height == 0)
    return;

  cleanupViewportResources();

  viewportExtent = {width, height};

  // Color image
  vk::ImageCreateInfo colorImageInfo{
      .imageType = vk::ImageType::e2D,
      .format = swapChainSurfaceFormat.format,
      .extent = {width, height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eColorAttachment |
               vk::ImageUsageFlagBits::eSampled,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};
  viewportColorImage = vk::raii::Image(device, colorImageInfo);

  auto colorMemReqs = viewportColorImage.getMemoryRequirements();
  vk::MemoryAllocateInfo colorAllocInfo{
      .allocationSize = colorMemReqs.size,
      .memoryTypeIndex =
          findMemoryType(colorMemReqs.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eDeviceLocal)};
  viewportColorImageMemory = vk::raii::DeviceMemory(device, colorAllocInfo);
  viewportColorImage.bindMemory(*viewportColorImageMemory, 0);

  vk::ImageViewCreateInfo colorViewInfo{
      .image = *viewportColorImage,
      .viewType = vk::ImageViewType::e2D,
      .format = swapChainSurfaceFormat.format,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  viewportColorImageView = vk::raii::ImageView(device, colorViewInfo);

  // Depth image
  vk::ImageCreateInfo depthImageInfo{
      .imageType = vk::ImageType::e2D,
      .format = depthFormat,
      .extent = {width, height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};
  viewportDepthImage = vk::raii::Image(device, depthImageInfo);

  auto depthMemReqs = viewportDepthImage.getMemoryRequirements();
  vk::MemoryAllocateInfo depthAllocInfo{
      .allocationSize = depthMemReqs.size,
      .memoryTypeIndex =
          findMemoryType(depthMemReqs.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eDeviceLocal)};
  viewportDepthImageMemory = vk::raii::DeviceMemory(device, depthAllocInfo);
  viewportDepthImage.bindMemory(*viewportDepthImageMemory, 0);

  vk::ImageViewCreateInfo depthViewInfo{
      .image = *viewportDepthImage,
      .viewType = vk::ImageViewType::e2D,
      .format = depthFormat,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  viewportDepthImageView = vk::raii::ImageView(device, depthViewInfo);

  // Sampler
  viewportSampler = vk::raii::Sampler(
      device, {.magFilter = vk::Filter::eLinear,
               .minFilter = vk::Filter::eLinear,
               .mipmapMode = vk::SamplerMipmapMode::eNearest,
               .addressModeU = vk::SamplerAddressMode::eClampToEdge,
               .addressModeV = vk::SamplerAddressMode::eClampToEdge,
               .addressModeW = vk::SamplerAddressMode::eClampToEdge});

  // ImGui descriptor set (only needed in editor mode)
  if (editorMode) {
    viewportDescriptorSet =
        ImGui_ImplVulkan_AddTexture(*viewportSampler, *viewportColorImageView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

void VulkanRenderingContext::cleanupViewportResources() {
  if (editorMode && viewportDescriptorSet != VK_NULL_HANDLE) {
    ImGui_ImplVulkan_RemoveTexture(viewportDescriptorSet);
    viewportDescriptorSet = VK_NULL_HANDLE;
  }
  viewportSampler = nullptr;
  viewportDepthImageView = nullptr;
  viewportDepthImage = nullptr;
  viewportDepthImageMemory = nullptr;
  viewportColorImageView = nullptr;
  viewportColorImage = nullptr;
  viewportColorImageMemory = nullptr;
}

void VulkanRenderingContext::recreateViewportIfNeeded() {
  if (pendingViewportExtent.width != viewportExtent.width ||
      pendingViewportExtent.height != viewportExtent.height) {
    device.waitIdle();
    createViewportResources(pendingViewportExtent.width,
                            pendingViewportExtent.height);
  }
}

void VulkanRenderingContext::createDescriptorPool() {
  constexpr uint32_t maxMaterials = 64;

  std::array<vk::DescriptorPoolSize, 3> poolSizes = {
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                             maxConcurrentFrames * 2},
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic,
                             maxConcurrentFrames},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                             maxMaterials * 4}};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames * 2 + maxMaterials,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
