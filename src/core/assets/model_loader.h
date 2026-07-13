#pragma once
#include "asset_loader.h"
#include "asset_manager.h"
#include "assets/obj_loader.h"
#include "rendering/mesh.h"
#include <fstream>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct MeshAsset {
  Mesh mesh;
};

class ModelLoader : public AssetLoader<MeshAsset> {
  vk::raii::Device &device;
  vk::raii::PhysicalDevice &physicalDevice;
  vk::raii::CommandPool &commandPool;
  vk::raii::Queue &queue;
  AssetManager &assetManager;

public:
  ModelLoader(vk::raii::Device &device,
              vk::raii::PhysicalDevice &physicalDevice,
              vk::raii::CommandPool &commandPool, vk::raii::Queue &queue,
              AssetManager &assetManager)
      : device(device), physicalDevice(physicalDevice),
        commandPool(commandPool), queue(queue),
        assetManager(assetManager) {}

  std::vector<std::string> extensions() const override {
    return {".obj", ".gltf", ".glb"};
  }

  MeshAsset loadAsset(const std::string &path) override {
    ObjData data = loadObj(path);

    Mesh mesh{};
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.vertexCount = static_cast<uint32_t>(data.vertices.size());
    if (auto *materials = assetManager.getLoader<MaterialAsset>()) {
      mesh.material = &materials->getAsset("brick.mat");
    }

    // Vertex buffer
    vk::DeviceSize vertexBufferSize = sizeof(Vertex) * data.vertices.size();
    vk::raii::Buffer vertexStagingBuffer{nullptr};
    vk::raii::DeviceMemory vertexStagingMemory{nullptr};
    createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 vertexStagingBuffer, vertexStagingMemory);

    void *mapped = vertexStagingMemory.mapMemory(0, vertexBufferSize);
    memcpy(mapped, data.vertices.data(), vertexBufferSize);
    vertexStagingMemory.unmapMemory();

    createBuffer(vertexBufferSize,
                 vk::BufferUsageFlagBits::eVertexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.vertexBuffer,
                 mesh.vertexMemory);

    copyBuffer(vertexStagingBuffer, mesh.vertexBuffer, vertexBufferSize);

    // Index buffer
    vk::DeviceSize indexBufferSize = sizeof(uint32_t) * data.indices.size();
    vk::raii::Buffer indexStagingBuffer{nullptr};
    vk::raii::DeviceMemory indexStagingMemory{nullptr};
    createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 indexStagingBuffer, indexStagingMemory);

    mapped = indexStagingMemory.mapMemory(0, indexBufferSize);
    memcpy(mapped, data.indices.data(), indexBufferSize);
    indexStagingMemory.unmapMemory();

    createBuffer(indexBufferSize,
                 vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.indexBuffer,
                 mesh.indexMemory);

    copyBuffer(indexStagingBuffer, mesh.indexBuffer, indexBufferSize);

    return MeshAsset{std::move(mesh)};
  }

private:
  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
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
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits, properties)};

    bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
  }

  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    auto copyCommandBuffer = std::move(commandBuffers.front());

    copyCommandBuffer.begin(
        {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
    copyCommandBuffer.copyBuffer(*srcBuffer, *dstBuffer, copyRegion);

    copyCommandBuffer.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*copyCommandBuffer};

    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
  }

  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties) {
    auto memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
              properties)
        return i;
    }
    throw std::runtime_error("failed to find suitable memory type");
  }
};
