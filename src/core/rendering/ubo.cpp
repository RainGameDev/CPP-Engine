#include "rendering/ubo.h"
#include "glm/ext/vector_float4.hpp"
#include "rendering/vulkan_render_context.h"

#include "assets/material_loader.h"
#include "vulkan/vulkan.hpp"
#include <chrono>
#include <cstdint>
#include <glm/ext/matrix_transform.hpp>

void VulkanRenderingContext::createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    uniformBuffers[i].buffer = vk::raii::Buffer(
        device, {.size = bufferSize,
                 .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                 .sharingMode = vk::SharingMode::eExclusive});

    vk::MemoryRequirements memRequirements =
        uniformBuffers[i].buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};

    uniformBuffers[i].memory = vk::raii::DeviceMemory(device, allocInfo);
    uniformBuffers[i].buffer.bindMemory(*uniformBuffers[i].memory, 0);
    uniformBuffers[i].mapped =
        uniformBuffers[i].memory.mapMemory(0, bufferSize);
  }

  vk::DeviceSize transformSize = transformStride;
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    transformBuffers[i].buffer = vk::raii::Buffer(
        device, {.size = transformSize,
                 .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                 .sharingMode = vk::SharingMode::eExclusive});

    vk::MemoryRequirements memRequirements =
        transformBuffers[i].buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};

    transformBuffers[i].memory = vk::raii::DeviceMemory(device, allocInfo);
    transformBuffers[i].buffer.bindMemory(*transformBuffers[i].memory, 0);
    transformBuffers[i].mapped =
        transformBuffers[i].memory.mapMemory(0, transformSize);
    transformBufferSizes[i] = transformSize;
  }

  vk::DeviceSize lightSize = sizeof(LightsUBO);
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    lightBuffers[i].buffer = vk::raii::Buffer(
        device, {.size = lightSize,
                 .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                 .sharingMode = vk::SharingMode::eExclusive});

    vk::MemoryRequirements memRequirements =
        lightBuffers[i].buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};

    lightBuffers[i].memory = vk::raii::DeviceMemory(device, allocInfo);
    lightBuffers[i].buffer.bindMemory(*lightBuffers[i].memory, 0);
    lightBuffers[i].mapped = lightBuffers[i].memory.mapMemory(0, lightSize);
  }

  vk::DeviceSize shadowSize = sizeof(ShadowUBO);
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    shadowBuffers[i].buffer = vk::raii::Buffer(
        device, {.size = shadowSize,
                 .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                 .sharingMode = vk::SharingMode::eExclusive});

    vk::MemoryRequirements memRequirements =
        shadowBuffers[i].buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};

    shadowBuffers[i].memory = vk::raii::DeviceMemory(device, allocInfo);
    shadowBuffers[i].buffer.bindMemory(*shadowBuffers[i].memory, 0);
    shadowBuffers[i].mapped = shadowBuffers[i].memory.mapMemory(0, shadowSize);
  }
}

void VulkanRenderingContext::updateShadowDescriptors() {
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    vk::DescriptorImageInfo shadowImageInfo{
        .sampler = *shadowSampler,
        .imageView = *shadowDepthView,
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal};
    vk::WriteDescriptorSet write{.dstSet = descriptorSets[i],
                                 .dstBinding = 4,
                                 .dstArrayElement = 0,
                                 .descriptorCount = 1,
                                 .descriptorType =
                                     vk::DescriptorType::eCombinedImageSampler,
                                 .pImageInfo = &shadowImageInfo};
    device.updateDescriptorSets(write, nullptr);
  }
}

void VulkanRenderingContext::createDescriptorSetLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 5> set0Bindings = {
      {{.binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr},
       {.binding = 1,
        .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr},
       {.binding = 2,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr},
       {.binding = 3,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr},
       {.binding = 4,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment}}};

  descriptorSetLayout = device.createDescriptorSetLayout(
      {.bindingCount = static_cast<uint32_t>(set0Bindings.size()),
       .pBindings = set0Bindings.data()});

  std::array<vk::DescriptorSetLayoutBinding, 4> materialBindings = {{
      {.binding = 0,
       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
       .descriptorCount = 1,
       .stageFlags = vk::ShaderStageFlagBits::eFragment},
      {.binding = 1,
       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
       .descriptorCount = 1,
       .stageFlags = vk::ShaderStageFlagBits::eFragment},
      {.binding = 2,
       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
       .descriptorCount = 1,
       .stageFlags = vk::ShaderStageFlagBits::eFragment},
      {.binding = 3,
       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
       .descriptorCount = 1,
       .stageFlags = vk::ShaderStageFlagBits::eFragment},
  }};

  materialSetLayout = device.createDescriptorSetLayout(
      {.bindingCount = static_cast<uint32_t>(materialBindings.size()),
       .pBindings = materialBindings.data()});
}

void VulkanRenderingContext::createDescriptorSets() {
  std::array<vk::DescriptorSetLayout, maxConcurrentFrames> layouts{};
  layouts.fill(*descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *descriptorPool,
                                          .descriptorSetCount =
                                              maxConcurrentFrames,
                                          .pSetLayouts = layouts.data()};

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *uniformBuffers[i].buffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject)};

    vk::DescriptorBufferInfo transformBufferInfo{
        .buffer = *transformBuffers[i].buffer,
        .offset = 0,
        .range = transformStride};

    vk::DescriptorBufferInfo lightBufferInfo{.buffer = *lightBuffers[i].buffer,
                                             .offset = 0,
                                             .range = sizeof(LightsUBO)};

    vk::DescriptorBufferInfo shadowBufferInfo{.buffer =
                                                  *shadowBuffers[i].buffer,
                                              .offset = 0,
                                              .range = sizeof(ShadowUBO)};

    vk::DescriptorImageInfo shadowImageInfo{
        .sampler = *shadowSampler,
        .imageView = *shadowDepthView,
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    std::array<vk::WriteDescriptorSet, 5> writes = {{
        {.dstSet = descriptorSets[i],
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &cameraBufferInfo},
        {.dstSet = descriptorSets[i],
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
         .pBufferInfo = &transformBufferInfo},
        {.dstSet = descriptorSets[i],
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &lightBufferInfo},
        {.dstSet = descriptorSets[i],
         .dstBinding = 3,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &shadowBufferInfo},
        {.dstSet = descriptorSets[i],
         .dstBinding = 4,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &shadowImageInfo},

    }};

    device.updateDescriptorSets(writes, nullptr);
  }
}

void VulkanRenderingContext::createDefaultMaterial() {
  // white albedo would decode to a tilted normal + full metal, so slots 1-2
  // get dedicated pixels: flat normal (128,128,255), rmaos (rough=1,metal=0,ao=1)
  auto makeSolid = [&](TextureAsset &out, uint32_t pixel) {
    vk::DeviceSize imageSize = sizeof(pixel);

    vk::BufferCreateInfo bufInfo{.size = imageSize,
                                 .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                 .sharingMode = vk::SharingMode::eExclusive};
    vk::raii::Buffer stagingBuf(device, bufInfo);

    auto memReq = stagingBuf.getMemoryRequirements();
    vk::MemoryAllocateInfo stagingAllocInfo{
        .allocationSize = memReq.size,
        .memoryTypeIndex =
            findMemoryType(memReq.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};
    vk::raii::DeviceMemory stagingMem(device, stagingAllocInfo);
    stagingBuf.bindMemory(*stagingMem, 0);

    void *mapped = stagingMem.mapMemory(0, imageSize);
    memcpy(mapped, &pixel, imageSize);
    stagingMem.unmapMemory();

    vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                  .format = vk::Format::eR8G8B8A8Srgb,
                                  .extent = {1, 1, 1},
                                  .mipLevels = 1,
                                  .arrayLayers = 1,
                                  .samples = vk::SampleCountFlagBits::e1,
                                  .tiling = vk::ImageTiling::eOptimal,
                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                           vk::ImageUsageFlagBits::eTransferDst,
                                  .sharingMode = vk::SharingMode::eExclusive,
                                  .initialLayout = vk::ImageLayout::eUndefined};
    out.image = vk::raii::Image(device, imageInfo);

    auto imgMemReq = out.image.getMemoryRequirements();
    vk::MemoryAllocateInfo imgAllocInfo{
        .allocationSize = imgMemReq.size,
        .memoryTypeIndex = findMemoryType(
            imgMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
    out.memory = vk::raii::DeviceMemory(device, imgAllocInfo);
    out.image.bindMemory(*out.memory, 0);

    {
      vk::CommandBufferBeginInfo beginInfo{
          .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
      auto cmdBufs = vk::raii::CommandBuffers(
          device, {.commandPool = *commandPool,
                   .level = vk::CommandBufferLevel::ePrimary,
                   .commandBufferCount = 1});
      auto cmd = std::move(cmdBufs.front());
      cmd.begin(beginInfo);

      vk::ImageMemoryBarrier2 barrier{
          .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
          .srcAccessMask = vk::AccessFlagBits2::eNone,
          .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
          .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
          .oldLayout = vk::ImageLayout::eUndefined,
          .newLayout = vk::ImageLayout::eTransferDstOptimal,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = *out.image,
          .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .baseMipLevel = 0,
                               .levelCount = 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1}};
      vk::DependencyInfo depInfo{.imageMemoryBarrierCount = 1,
                                 .pImageMemoryBarriers = &barrier};
      cmd.pipelineBarrier2(depInfo);

      vk::BufferImageCopy region{
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .mipLevel = 0,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
          .imageOffset = {0, 0, 0},
          .imageExtent = {1, 1, 1}};
      cmd.copyBufferToImage(*stagingBuf, *out.image,
                            vk::ImageLayout::eTransferDstOptimal, region);

      barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
      barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
      cmd.pipelineBarrier2(depInfo);

      cmd.end();

      vk::CommandBuffer rawCmd = *cmd;
      vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                                .pCommandBuffers = &rawCmd};
      queue.submit(submitInfo, nullptr);
      queue.waitIdle();
    }

    out.imageView = vk::raii::ImageView(
        device,
        vk::ImageViewCreateInfo{
            .image = *out.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .levelCount = 1,
                                 .layerCount = 1}});

    out.sampler = vk::raii::Sampler(
        device,
        vk::SamplerCreateInfo{.magFilter = vk::Filter::eNearest,
                              .minFilter = vk::Filter::eNearest,
                              .mipmapMode = vk::SamplerMipmapMode::eNearest,
                              .addressModeU = vk::SamplerAddressMode::eRepeat,
                              .addressModeV = vk::SamplerAddressMode::eRepeat});
  };

  makeSolid(defaultWhiteTexture, 0xFFFFFFFF);
  makeSolid(defaultNormalTexture, 0xFFFF8080);
  makeSolid(defaultRmaosTexture, 0xFFFF00FF);

  vk::DescriptorSetLayout layout = materialSetLayout;
  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *descriptorPool,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts = &layout};
  auto sets = device.allocateDescriptorSets(allocInfo);
  defaultMaterialDescriptorSet = std::move(sets.front());

  vk::DescriptorImageInfo slotInfos[4] = {
      {.sampler = *defaultWhiteTexture.sampler,
       .imageView = *defaultWhiteTexture.imageView,
       .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal},
      {.sampler = *defaultNormalTexture.sampler,
       .imageView = *defaultNormalTexture.imageView,
       .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal},
      {.sampler = *defaultRmaosTexture.sampler,
       .imageView = *defaultRmaosTexture.imageView,
       .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal},
      {.sampler = *defaultWhiteTexture.sampler,
       .imageView = *defaultWhiteTexture.imageView,
       .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}};

  std::array<vk::WriteDescriptorSet, 4> writes{};
  for (uint32_t i = 0; i < 4; ++i) {
    writes[i] = {.dstSet = *defaultMaterialDescriptorSet,
                 .dstBinding = i,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                 .pImageInfo = &slotInfos[i]};
  }
  device.updateDescriptorSets(writes, nullptr);
}
