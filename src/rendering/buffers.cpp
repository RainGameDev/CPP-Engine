#include "rendering/vulkan_application.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

void VulkanApplication::createBuffer(vk::DeviceSize size,
                                     vk::BufferUsageFlags usage,
                                     vk::MemoryPropertyFlags properties,
                                     vk::raii::Buffer &buffer,
                                     vk::raii::DeviceMemory &bufferMemory) {
  vk::BufferCreateInfo bufferInfo{.size = size,
                                  .usage = usage,
                                  .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device, bufferInfo);

  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                        properties)};

  bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void VulkanApplication::copyBuffer(vk::raii::Buffer &srcBuffer,
                                   vk::raii::Buffer &dstBuffer,
                                   vk::DeviceSize size) {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};

  auto commandBuffers =
      vk::raii::CommandBuffers(device, allocInfo);
  auto copyCommandBuffer = std::move(commandBuffers.front());

  copyCommandBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
  copyCommandBuffer.copyBuffer(*srcBuffer, *dstBuffer, copyRegion);

  copyCommandBuffer.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*copyCommandBuffer};

  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}

void VulkanApplication::createCubeMesh() {
  std::vector<Vertex> vertices = {
      // --- FRONT FACE (+Z) ---
      {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},

      // --- BACK FACE (-Z) ---
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
      {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},

      // --- LEFT FACE (-X) ---
      {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},

      // --- RIGHT FACE (+X) ---
      {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},
      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},

      // --- TOP FACE (+Y) ---
      {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},

      // --- BOTTOM FACE (-Y) ---
      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  };

  std::vector<uint32_t> indices = {
      0,  1,  2,  0,  2,  3,   // Front
      4,  5,  6,  4,  6,  7,   // Back
      8,  9,  10, 8,  10, 11,  // Left
      12, 13, 14, 12, 14, 15,  // Right
      16, 17, 18, 16, 18, 19,  // Top
      20, 21, 22, 20, 22, 23,  // Bottom
  };

  Mesh cube{};

  vk::DeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
  vk::raii::Buffer vertexStagingBuffer{nullptr};
  vk::raii::DeviceMemory vertexStagingMemory{nullptr};
  createBuffer(vertexBufferSize,
               vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vertexStagingBuffer, vertexStagingMemory);

  void *mapped = vertexStagingMemory.mapMemory(0, vertexBufferSize);
  memcpy(mapped, vertices.data(), vertexBufferSize);
  vertexStagingMemory.unmapMemory();

  createBuffer(
      vertexBufferSize,
      vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal, cube.vertexBuffer,
      cube.vertexMemory);

  copyBuffer(vertexStagingBuffer, cube.vertexBuffer, vertexBufferSize);

  vk::DeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
  vk::raii::Buffer indexStagingBuffer{nullptr};
  vk::raii::DeviceMemory indexStagingMemory{nullptr};
  createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               indexStagingBuffer, indexStagingMemory);

  mapped = indexStagingMemory.mapMemory(0, indexBufferSize);
  memcpy(mapped, indices.data(), indexBufferSize);
  indexStagingMemory.unmapMemory();

  createBuffer(
      indexBufferSize,
      vk::BufferUsageFlagBits::eIndexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal, cube.indexBuffer,
      cube.indexMemory);

  copyBuffer(indexStagingBuffer, cube.indexBuffer, indexBufferSize);

  cube.indexCount = static_cast<uint32_t>(indices.size());
  meshes.push_back(std::move(cube));
}
