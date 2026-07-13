#include "glm/ext/vector_float4.hpp"
#include "rendering/vulkan_render_context.h"

#include "assets/material_loader.h"
#include <chrono>
#include <cstdint>
#include <glm/ext/matrix_transform.hpp>

void VulkanRenderingContext::createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    uniformBuffers[i].buffer = vk::raii::Buffer(device, {.size = bufferSize,
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

  vk::DeviceSize transformSize = sizeof(TransformUBO);
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    transformBuffers[i].buffer = vk::raii::Buffer(device, {.size = transformSize,
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
    lightBuffers[i].buffer = vk::raii::Buffer(device, {.size = lightSize,
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
    lightBuffers[i].mapped =
        lightBuffers[i].memory.mapMemory(0, lightSize);
  }
}

void VulkanRenderingContext::createDescriptorSetLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 3> set0Bindings = {{
       {.binding = 0,
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
  }};

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
        .range = sizeof(TransformUBO)};

    vk::DescriptorBufferInfo lightBufferInfo{
        .buffer = *lightBuffers[i].buffer,
        .offset = 0,
        .range = sizeof(LightsUBO)};

    std::array<vk::WriteDescriptorSet, 3> writes = {{
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
    }};

    device.updateDescriptorSets(writes, nullptr);
  }
}


