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
#include "rendering/ubo.h"
#include "vulkan/vulkan.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

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
  createShadowResources();
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
  cleanupShadowResources();
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

namespace {
int shadowTypePriority(const LightType &t) {
  if (std::holds_alternative<Directional>(t))
    return 0;
  if (std::holds_alternative<Spot>(t))
    return 1;
  return 2;
}

glm::vec3 shadowUpFor(glm::vec3 dir) {
  return fabs(glm::dot(dir, glm::vec3(0, 1, 0))) > 0.99f ? glm::vec3(0, 0, 1)
                                                         : glm::vec3(0, 1, 0);
}

glm::vec3 safeNormalize(glm::vec3 v, glm::vec3 fallback) {
  float len = glm::length(v);
  return len > 1e-6f ? v / len : fallback;
}

ShadowData makeShadowData(LightComponent &lc, TransformComponent &tc) {
  ShadowData d{};
  glm::vec3 fwd = tc.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 dir = safeNormalize(fwd, glm::vec3(0, -1, 0));

  if (std::holds_alternative<Directional>(lc.lightType)) {
    float extent = lc.shadowExtent > 0.0f ? lc.shadowExtent : 20.0f;
    float nearP = lc.shadowNear > 0.0f ? lc.shadowNear : 0.5f;
    float farP = lc.shadowFar > nearP ? lc.shadowFar : nearP + 1.0f;
    glm::vec3 pos = -dir * 40.0f;
    glm::mat4 proj =
        glm::orthoZO(-extent, extent, -extent, extent, nearP, farP);
    proj[1][1] *= -1;
    glm::mat4 view = glm::lookAt(pos, glm::vec3(0), shadowUpFor(dir));
    d.viewProj = proj * view;
    d.pos_far = glm::vec4(pos, farP);
    d.dir_type = glm::vec4(dir, 0.0f);
  } else if (std::holds_alternative<Spot>(lc.lightType)) {
    auto &spot = std::get<Spot>(lc.lightType);
    glm::vec3 pos = tc.position;
    float nearP = lc.shadowNear > 0.0f ? lc.shadowNear : 0.1f;
    float farP = spot.length > nearP ? spot.length : nearP + 1.0f;
    float fov = glm::clamp(spot.angle + 2.0f, 1.0f, 179.0f);
    glm::mat4 proj = glm::perspectiveZO(glm::radians(fov), 1.0f, nearP, farP);
    proj[1][1] *= -1;
    glm::mat4 view = glm::lookAt(pos, pos + dir, shadowUpFor(dir));
    d.viewProj = proj * view;
    d.pos_far = glm::vec4(pos, farP);
    d.dir_type = glm::vec4(dir, 1.0f);
  } else {
    glm::vec3 pos = tc.position;
    float farP = std::get<Point>(lc.lightType).radius;
    if (farP <= 0.0f)
      farP = lc.shadowFar;
    d.viewProj = glm::mat4(1.0f);
    d.pos_far = glm::vec4(pos, farP);
    d.dir_type = glm::vec4(dir, 2.0f);
  }
  return d;
}
} // namespace
void VulkanRenderingContext::updateLightBuffer(uint32_t frame) {
  struct Entry {
    LightComponent *lc;
    TransformComponent *tc;
  };
  std::vector<Entry> entries;
  Query<LightComponent, TransformComponent> lightQuery(*world);
  lightQuery.for_each(
      [&](EntityId, LightComponent &lc, TransformComponent &tc) {
        if (entries.size() >= MAX_LIGHTS)
          return;
        entries.push_back({&lc, &tc});
      });

  // Assign shadow slots in priority order (dir > spot > point)
  std::vector<int> order(entries.size());
  for (size_t i = 0; i < order.size(); ++i)
    order[i] = (int)i;
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    return shadowTypePriority(entries[a].lc->lightType) <
           shadowTypePriority(entries[b].lc->lightType);
  });
  std::vector<int> shadowIdx(entries.size(), -1);
  int sCount = 0;
  for (int i : order) {
    if (entries[i].lc->castShadow && sCount < (int)MAX_SHADOWS)
      shadowIdx[i] = sCount++;
  }

  LightsUBO lightsubo{};
  for (size_t i = 0; i < entries.size(); ++i) {
    auto *lc = entries[i].lc;
    auto *tc = entries[i].tc;
    auto &l = lightsubo.lights[lightsubo.count];
    glm::vec3 forward = tc->rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    if (std::holds_alternative<Directional>(lc->lightType)) {
      l.positionOrDirection = glm::vec4(glm::normalize(tc->position), 0.0f);
      l.direction = glm::vec4(forward, 0.0f);
    } else if (std::holds_alternative<Point>(lc->lightType)) {
      l.positionOrDirection = glm::vec4(tc->position, 1.0f);
      l.params.x = std::get<Point>(lc->lightType).radius;
      l.direction = glm::vec4(forward, 1.0f);
    } else if (std::holds_alternative<Spot>(lc->lightType)) {
      l.positionOrDirection = glm::vec4(tc->position, 2.0f);
      l.direction = glm::vec4(forward, 1.0f);
      auto &spot = std::get<Spot>(lc->lightType);
      l.params.x = spot.angle;
      l.params.y = spot.length;
    }
    l.colorAndIntensity = glm::vec4(lc->color, lc->intensity);
    l.shadowInfo = glm::vec4((float)shadowIdx[i], lc->shadowBias,
                             lc->shadowNear, lc->shadowFar);
    lightsubo.count++;
  }

  memcpy(lightBuffers[frame].mapped, &lightsubo, sizeof(lightsubo));
}

void VulkanRenderingContext::updateShadowBuffer(uint32_t frame) {
  struct Entry {
    LightComponent *lc;
    TransformComponent *tc;
  };
  std::vector<Entry> entries;
  Query<LightComponent, TransformComponent> lightQuery(*world);
  lightQuery.for_each(
      [&](EntityId, LightComponent &lc, TransformComponent &tc) {
        entries.push_back({&lc, &tc});
      });
  std::stable_sort(entries.begin(), entries.end(),
                   [](const Entry &a, const Entry &b) {
                     return shadowTypePriority(a.lc->lightType) <
                            shadowTypePriority(b.lc->lightType);
                   });

  ShadowUBO shadowubo{};
  shadowubo.shadows[0].viewProj = glm::mat4(1.0f);
  shadowubo.count = 0;
  for (auto &e : entries) {
    if (!e.lc->castShadow)
      continue;
    if (shadowubo.count >= MAX_SHADOWS)
      break;
    shadowubo.shadows[shadowubo.count] = makeShadowData(*e.lc, *e.tc);
    shadowubo.count++;
  }

  memcpy(shadowBuffers[frame].mapped, &shadowubo, sizeof(shadowubo));
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

void VulkanRenderingContext::createShadowResources() {
  shadowLayerViews.clear();

  // create image 2D array, one layer per shadow slot
  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = depthFormat,
      .extent = {shadowExtent.width, shadowExtent.height, 1},
      .mipLevels = 1,
      .arrayLayers = MAX_SHADOWS,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
               vk::ImageUsageFlagBits::eSampled,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};

  shadowDepthImage = vk::raii::Image(device, imageInfo);
  auto memReqs = shadowDepthImage.getMemoryRequirements();

  // bind memory
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReqs.size,
      .memoryTypeIndex = findMemoryType(
          memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
  shadowDepthMemory = vk::raii::DeviceMemory(device, allocInfo);
  shadowDepthImage.bindMemory(*shadowDepthMemory, 0);

  // array view for sampling in frag shaders
  vk::ImageViewCreateInfo arrayViewInfo{
      .image = *shadowDepthImage,
      .viewType = vk::ImageViewType::e2DArray,
      .format = depthFormat,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = MAX_SHADOWS}};
  shadowDepthView = vk::raii::ImageView(device, arrayViewInfo);

  // perlayer views for depth rendering
  for (uint32_t i = 0; i < MAX_SHADOWS; ++i) {
    vk::ImageViewCreateInfo layerViewInfo{
        .image = *shadowDepthImage,
        .viewType = vk::ImageViewType::e2D,
        .format = depthFormat,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = i,
                             .layerCount = 1}};
    shadowLayerViews.emplace_back(device, layerViewInfo);
  }

  // create sampler
  shadowSampler = vk::raii::Sampler(
      device, {.magFilter = vk::Filter::eLinear,
               .minFilter = vk::Filter::eLinear,
               .mipmapMode = vk::SamplerMipmapMode::eNearest,
               .addressModeU = vk::SamplerAddressMode::eClampToBorder,
               .addressModeV = vk::SamplerAddressMode::eClampToBorder,
               .addressModeW = vk::SamplerAddressMode::eClampToEdge,
               .borderColor = vk::BorderColor::eFloatOpaqueWhite});
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

void VulkanRenderingContext::cleanupShadowResources() {
  shadowSampler = nullptr;
  shadowLayerViews.clear();
  shadowDepthView = nullptr;
  shadowDepthImage = nullptr;
  shadowDepthMemory = nullptr;
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
                             maxConcurrentFrames * 3}, // camera+lights+shadow
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic,
                             maxConcurrentFrames}, // transform
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                             maxMaterials * 4 + maxConcurrentFrames + 4}};
  // 256 mats +2 shadow +4 default white

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames + maxMaterials +
                 2, // 2 set0 +64 +1 default +1 spare = 68
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
