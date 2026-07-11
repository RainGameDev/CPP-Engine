#include "glm/ext/vector_float4.hpp"
#include "rendering/vulkan_application.h"

#include "assets/material_loader.h"
#include <chrono>
#include <cstdint>
#include <glm/ext/matrix_transform.hpp>

void VulkanApplication::createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
  // Create the buffer
  vk::BufferCreateInfo bufferInfo{.size = bufferSize,
                                  .usage =
                                      vk::BufferUsageFlagBits::eUniformBuffer,
                                  .sharingMode = vk::SharingMode::eExclusive};
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    uniformBuffers[i].buffer = vk::raii::Buffer(device, bufferInfo);

    // Allocate and bind memory
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

    // Persistently map the buffer memory
    uniformBuffers[i].mapped =
        uniformBuffers[i].memory.mapMemory(0, bufferSize);
  }
}

void VulkanApplication::createDescriptorSetLayout() {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .pImmutableSamplers = nullptr};

  descriptorSetLayout = device.createDescriptorSetLayout(
      {.bindingCount = 1, .pBindings = &uboLayoutBinding});

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

void VulkanApplication::createDescriptorSets() {
  std::array<vk::DescriptorSetLayout, maxConcurrentFrames> layouts{};
  layouts.fill(*descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *descriptorPool,
                                          .descriptorSetCount =
                                              maxConcurrentFrames,
                                          .pSetLayouts = layouts.data()};

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    vk::DescriptorBufferInfo bufferInfo{.buffer = *uniformBuffers[i].buffer,
                                        .offset = 0,
                                        .range = sizeof(UniformBufferObject)};

    vk::WriteDescriptorSet descriptorWrite{
        .dstSet = descriptorSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &bufferInfo};

    device.updateDescriptorSets(descriptorWrite, nullptr);
  }
}

void VulkanApplication::updateUniformBuffer(uint32_t currentFrame) {

  UniformBufferObject ubo{};

  ubo.model = glm::mat4(1.0f);

  // View matrix: get from our camera
  ubo.view = camera.getViewMatrix();

  ubo.pos = glm::vec4(camera.position, 0.0);

  // Projection matrix: get from our camera
  ubo.proj = camera.getProjectionMatrix(swapChainExtent.width /
                                        (float)swapChainExtent.height);

  // Vulkan's Y coordinate is inverted compared to OpenGL
  ubo.proj[1][1] *= -1;

  // Copy the data to the uniform buffer for the current frame-in-flight
  memcpy(uniformBuffers[currentFrame].mapped, &ubo, sizeof(ubo));
}
