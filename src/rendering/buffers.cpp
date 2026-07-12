#include "assets/material_loader.h"
#include "assets/obj_loader.h"
#include "ecs/components.h"
#include "rendering/vulkan_render_context.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

void VulkanRenderingContext::createBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer,
    vk::raii::DeviceMemory &bufferMemory) {
  vk::BufferCreateInfo bufferInfo{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device, bufferInfo);

  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits, properties)};

  bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void VulkanRenderingContext::copyBuffer(vk::raii::Buffer &srcBuffer,
                                        vk::raii::Buffer &dstBuffer,
                                        vk::DeviceSize size) {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
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

void VulkanRenderingContext::createCubeMesh() {
  constexpr uint32_t SUBDIVISIONS = 32;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // Each face: 4 corners, normal, tangent, 2 tangent-space axes
  struct FaceDef {
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    // corner positions in winding order
    glm::vec3 corners[4];
    // UV for each corner
    glm::vec2 uvs[4];
  };

  FaceDef faces[6] = {
      // Front (+Z)
      {{0, 0, 1},
       {1, 0, 0},
       {0, 1, 0},
       {{-0.5f, -0.5f, 0.5f},
        {0.5f, -0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f, 0.5f}},
       {{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
      // Back (-Z)
      {{0, 0, -1},
       {-1, 0, 0},
       {0, 1, 0},
       {{0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f}},
       {{0, 1}, {1, 1}, {1, 0}, {0, 0}}},
      // Left (-X)
      {{-1, 0, 0},
       {0, 0, 1},
       {0, 1, 0},
       {{-0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f, -0.5f}},
       {{0, 1}, {1, 1}, {1, 0}, {0, 0}}},
      // Right (+X)
      {{1, 0, 0},
       {0, 0, -1},
       {0, 1, 0},
       {{0.5f, -0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f},
        {0.5f, 0.5f, 0.5f}},
       {{0, 1}, {1, 1}, {1, 0}, {0, 0}}},
      // Top (+Y)
      {{0, 1, 0},
       {1, 0, 0},
       {0, 0, 1},
       {{-0.5f, 0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f},
        {0.5f, 0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f}},
       {{0, 1}, {1, 1}, {1, 0}, {0, 0}}},
      // Bottom (-Y)
      {{0, -1, 0},
       {-1, 0, 0},
       {0, 0, 1},
       {{0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f}},
       {{0, 1}, {1, 1}, {1, 0}, {0, 0}}},
  };

  for (uint32_t f = 0; f < 6; f++) {
    auto &face = faces[f];
    uint32_t base = static_cast<uint32_t>(vertices.size());

    for (uint32_t j = 0; j <= SUBDIVISIONS; j++) {
      for (uint32_t i = 0; i <= SUBDIVISIONS; i++) {
        float u = static_cast<float>(i) / SUBDIVISIONS;
        float v = static_cast<float>(j) / SUBDIVISIONS;

        // Bilinear interpolation of position
        glm::vec3 p = (1 - u) * (1 - v) * face.corners[0] +
                      u * (1 - v) * face.corners[1] + u * v * face.corners[2] +
                      (1 - u) * v * face.corners[3];

        // Bilinear interpolation of UV
        glm::vec2 uv = (1 - u) * (1 - v) * face.uvs[0] +
                       u * (1 - v) * face.uvs[1] + u * v * face.uvs[2] +
                       (1 - u) * v * face.uvs[3];

        vertices.push_back({p, {1, 1, 1}, uv, face.tangent, face.normal});
      }
    }

    for (uint32_t j = 0; j < SUBDIVISIONS; j++) {
      for (uint32_t i = 0; i < SUBDIVISIONS; i++) {
        uint32_t a = base + j * (SUBDIVISIONS + 1) + i;
        uint32_t b = a + 1;
        uint32_t c = a + (SUBDIVISIONS + 1);
        uint32_t d = c + 1;
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(d);
        indices.push_back(a);
        indices.push_back(d);
        indices.push_back(c);
      }
    }
  }

  auto *materials = assetManager.getLoader<MaterialAsset>();
  Mesh cube{};

  vk::DeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
  vk::raii::Buffer vertexStagingBuffer{nullptr};
  vk::raii::DeviceMemory vertexStagingMemory{nullptr};
  createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vertexStagingBuffer, vertexStagingMemory);

  void *mapped = vertexStagingMemory.mapMemory(0, vertexBufferSize);
  memcpy(mapped, vertices.data(), vertexBufferSize);
  vertexStagingMemory.unmapMemory();

  createBuffer(vertexBufferSize,
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

  createBuffer(indexBufferSize,
               vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, cube.indexBuffer,
               cube.indexMemory);

  copyBuffer(indexStagingBuffer, cube.indexBuffer, indexBufferSize);

  cube.vertexCount = static_cast<uint32_t>(vertices.size());
  cube.indexCount = static_cast<uint32_t>(indices.size());
  cube.material = &materials->getAsset("brick.mat");

  auto entity = world->create_entity();
  world->add_component(entity, MeshComponent{.mesh = std::move(cube)});
  world->add_component(entity, TransformComponent{});
}

Mesh VulkanRenderingContext::createIndicatorMesh() {
  float s = 0.5f;
  std::vector<Vertex> vertices = {
      // Top pyramid
      {{0, s, 0}, {1, 1, 1}, {0, 0}, {1, 0, 0}, {0, 1, 0}},
      {{-s, 0, -s}, {1, 1, 1}, {0, 0}, {-1, 0, 0}, {-1, 1, -1}},
      {{s, 0, -s}, {1, 1, 1}, {0, 0}, {0, 0, -1}, {0, 1, -1}},
      {{s, 0, s}, {1, 1, 1}, {0, 0}, {1, 0, 0}, {1, 1, 0}},
      {{-s, 0, s}, {1, 1, 1}, {0, 0}, {0, 0, 1}, {0, 1, 1}},
      // Bottom pyramid
      {{0, -s, 0}, {1, 1, 1}, {0, 0}, {0, -1, 0}, {0, -1, 0}},
      {{-s, 0, -s}, {1, 1, 1}, {0, 0}, {-1, 0, 0}, {-1, 1, -1}},
      {{s, 0, -s}, {1, 1, 1}, {0, 0}, {0, 0, -1}, {0, 1, -1}},
      {{s, 0, s}, {1, 1, 1}, {0, 0}, {1, 0, 0}, {1, 1, 0}},
      {{-s, 0, s}, {1, 1, 1}, {0, 0}, {0, 0, 1}, {0, 1, 1}},
  };

  // Rebuild proper normals per face
  auto fixNormal = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) -> glm::vec3 {
    return glm::normalize(glm::cross(b - a, c - a));
  };

  struct Face {
    uint32_t i[3];
  };
  std::vector<Face> faces = {
      // Top
      {0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 1},
      // Bottom
      {5, 7, 6}, {5, 8, 7}, {5, 9, 8}, {5, 6, 9},
  };

  std::vector<uint32_t> indices;
  for (auto &f : faces) {
    glm::vec3 n = fixNormal(vertices[f.i[0]].pos, vertices[f.i[1]].pos,
                            vertices[f.i[2]].pos);
    for (uint32_t i = 0; i < 3; i++) {
      vertices[f.i[i]].normal = n;
      indices.push_back(f.i[i]);
    }
  }

  auto *materials = assetManager.getLoader<MaterialAsset>();
  Mesh mesh{};

  vk::DeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
  vk::raii::Buffer vertexStagingBuffer{nullptr};
  vk::raii::DeviceMemory vertexStagingMemory{nullptr};
  createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vertexStagingBuffer, vertexStagingMemory);

  void *mapped = vertexStagingMemory.mapMemory(0, vertexBufferSize);
  memcpy(mapped, vertices.data(), vertexBufferSize);
  vertexStagingMemory.unmapMemory();

  createBuffer(vertexBufferSize,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.vertexBuffer,
               mesh.vertexMemory);

  copyBuffer(vertexStagingBuffer, mesh.vertexBuffer, vertexBufferSize);

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

  createBuffer(indexBufferSize,
               vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.indexBuffer,
               mesh.indexMemory);

  copyBuffer(indexStagingBuffer, mesh.indexBuffer, indexBufferSize);

  mesh.vertexCount = static_cast<uint32_t>(vertices.size());
  mesh.indexCount = static_cast<uint32_t>(indices.size());
  mesh.material = &materials->getAsset("debug.mat");

  return mesh;
}

Mesh VulkanRenderingContext::loadObjMesh(const std::string &objPath,
                                         MaterialAsset *material) {
  ObjData data = loadObj(objPath);

  Mesh mesh{};
  mesh.indexCount = static_cast<uint32_t>(data.indices.size());
  mesh.vertexCount = static_cast<uint32_t>(data.vertices.size());
  mesh.material = material;

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

  return mesh;
}
