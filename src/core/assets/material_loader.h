#pragma once
#include "asset_loader.h"
#include "asset_manager.h"
#include "texture_loader.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

struct MaterialPushConstants {
  glm::vec4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  float parallaxStrength;
};

struct MaterialAsset {
  std::string vertexShader;
  std::string fragmentShader;
  std::string albedoPath;
  std::string normalPath;
  std::string rmaosPath;
  std::string parallaxPath;
  float parallaxStrength;
  glm::vec4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  vk::raii::DescriptorSet descriptorSet{nullptr};
};

class MaterialLoader : public AssetLoader<MaterialAsset> {
  vk::raii::Device &device;
  vk::raii::PhysicalDevice &physicalDevice;
  vk::DescriptorSetLayout materialLayout;
  vk::raii::DescriptorPool &descriptorPool;
  vk::raii::CommandPool &commandPool;
  vk::raii::Queue &queue;
  AssetManager &assetManager;

  // white fallback texture for missing maps
  TextureAsset fallbackTexture;

public:
  MaterialLoader(vk::raii::Device &device,
                 vk::raii::PhysicalDevice &physicalDevice,
                 vk::DescriptorSetLayout materialLayout,
                 vk::raii::DescriptorPool &descriptorPool,
                 vk::raii::CommandPool &commandPool, vk::raii::Queue &queue,
                 AssetManager &assetManager)
      : device(device), physicalDevice(physicalDevice),
        materialLayout(materialLayout), descriptorPool(descriptorPool),
        commandPool(commandPool), queue(queue), assetManager(assetManager) {
    createFallbackTexture();
  }

  std::vector<std::string> extensions() const override { return {".mat.json"}; }

  MaterialAsset loadAsset(const std::string &path) override {
    //  Parse JSON
    std::ifstream file(path);
    if (!file.is_open())
      throw std::runtime_error("failed to open material: " + path);
    nlohmann::json json;
    file >> json;

    // Convert JSON to asset
    MaterialAsset mat;
    mat.fragmentShader = json.value("fragmentShader", "sdr_default_model.frag");
    mat.vertexShader = json.value("vertexShader", "sdr_default_model.vert");
    mat.albedoPath = json.value("albedoMap", "");
    mat.normalPath = json.value("normalMap", "");
    mat.rmaosPath = json.value("rmaosMap", "");
    mat.parallaxPath = json.value("heightMap", "");
    mat.parallaxStrength =
        mat.parallaxPath.empty() ? 0.0f : json.value("parallaxStrength", 0.05f);
    mat.baseColorFactor =
        parseVec4(json, "baseColorFactor", {1.0f, 1.0f, 1.0f, 1.0f});
    mat.metallicFactor = json.value("metallicFactor", 1.0f);
    mat.roughnessFactor = json.value("roughnessFactor", 1.0f);

    // Look up textures (or use fallback)
    auto *textures = assetManager.getLoader<TextureAsset>();
    auto getTexture = [&](const std::string &path) -> TextureAsset & {
      if (path.empty())
        return fallbackTexture;
      // Strip directory and extension: "textures/dirt_albedo.png" ->
      // "dirt_albedo"
      std::filesystem::path p(path);
      auto *t = textures->tryGetAsset(p.stem().string());
      return t ? *t : fallbackTexture;
    };

    auto &albedo = getTexture(mat.albedoPath);
    auto &normal = getTexture(mat.normalPath);
    auto &rmaos = getTexture(mat.rmaosPath);
    auto &heightTex = getTexture(mat.parallaxPath);

    // Allocate descriptor set
    vk::DescriptorSetLayout layouts[] = {materialLayout};
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                            .descriptorSetCount = 1,
                                            .pSetLayouts = layouts};
    auto sets = vk::raii::DescriptorSets(device, allocInfo);
    mat.descriptorSet = std::move(sets.front());

    // Write descriptor set
    auto writeImageInfo = [](TextureAsset &tex) {
      return vk::DescriptorImageInfo{
          .sampler = *tex.sampler,
          .imageView = *tex.imageView,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    };

    vk::DescriptorImageInfo albedoInfo = writeImageInfo(albedo);
    vk::DescriptorImageInfo normalInfo = writeImageInfo(normal);
    vk::DescriptorImageInfo rmaosInfo = writeImageInfo(rmaos);
    vk::DescriptorImageInfo heightInfo = writeImageInfo(heightTex);

    std::array<vk::WriteDescriptorSet, 4> writes{{
        // Albedo
        {.dstSet = *mat.descriptorSet,
         .dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &albedoInfo},
        // Normal
        {.dstSet = *mat.descriptorSet,
         .dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &normalInfo},
        // RMAOS
        {.dstSet = *mat.descriptorSet,
         .dstBinding = 2,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &rmaosInfo},
        // Height (parallax)
        {.dstSet = *mat.descriptorSet,
         .dstBinding = 3,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &heightInfo},
    }};
    device.updateDescriptorSets(writes, nullptr);

    return mat;
  }

private:
  glm::vec4 parseVec4(const nlohmann::json &j, const std::string &key,
                      glm::vec4 fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() != 4)
      return fallback;
    return {j[key][0], j[key][1], j[key][2], j[key][3]};
  }

  void createFallbackTexture() {
    uint32_t pixel = 0xFFFFFFFF;
    vk::DeviceSize imageSize = sizeof(pixel);

    // Create staging buffer
    vk::BufferCreateInfo bufInfo{.size = imageSize,
                                 .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                 .sharingMode = vk::SharingMode::eExclusive};
    vk::raii::Buffer stagingBuf(device, bufInfo);

    auto memReq = stagingBuf.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memReq.size,
        .memoryTypeIndex =
            findMemoryType(memReq.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};
    vk::raii::DeviceMemory stagingMem(device, allocInfo);
    stagingBuf.bindMemory(*stagingMem, 0);

    void *mapped = stagingMem.mapMemory(0, imageSize);
    memcpy(mapped, &pixel, imageSize);
    stagingMem.unmapMemory();

    // Create 1x1 image
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
    fallbackTexture.image = vk::raii::Image(device, imageInfo);

    auto imgMemReq = fallbackTexture.image.getMemoryRequirements();
    vk::MemoryAllocateInfo imgAllocInfo{
        .allocationSize = imgMemReq.size,
        .memoryTypeIndex =
            findMemoryType(imgMemReq.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eDeviceLocal)};
    fallbackTexture.memory = vk::raii::DeviceMemory(device, imgAllocInfo);
    fallbackTexture.image.bindMemory(*fallbackTexture.memory, 0);

    // Transition Undefined -> TransferDst, copy, transition -> ShaderReadOnly
    transitionImage(fallbackTexture.image, vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuf, fallbackTexture.image, 1, 1);
    transitionImage(fallbackTexture.image, vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal);

    // ImageView
    fallbackTexture.imageView = vk::raii::ImageView(
        device,
        vk::ImageViewCreateInfo{
            .image = *fallbackTexture.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .levelCount = 1,
                                 .layerCount = 1}});

    // Sampler
    fallbackTexture.sampler = vk::raii::Sampler(
        device,
        vk::SamplerCreateInfo{.magFilter = vk::Filter::eNearest,
                              .minFilter = vk::Filter::eNearest,
                              .mipmapMode = vk::SamplerMipmapMode::eNearest,
                              .addressModeU = vk::SamplerAddressMode::eRepeat,
                              .addressModeV = vk::SamplerAddressMode::eRepeat});

    fallbackTexture.width = 1;
    fallbackTexture.height = 1;
    fallbackTexture.mipLevels = 1;
    fallbackTexture.format = vk::Format::eR8G8B8A8Srgb;
  }

  void transitionImage(vk::raii::Image &image, vk::ImageLayout oldLayout,
                       vk::ImageLayout newLayout) {
    auto cmdBufs = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(cmdBufs.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eNone,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *image,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .levelCount = 1,
                             .layerCount = 1}};
    vk::DependencyInfo depInfo{.imageMemoryBarrierCount = 1,
                               .pImageMemoryBarriers = &barrier};
    cmd.pipelineBarrier2(depInfo);
    cmd.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*cmd};
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
  }

  void copyBufferToImage(vk::raii::Buffer &buffer, vk::raii::Image &image,
                         uint32_t w, uint32_t h) {
    auto cmdBufs = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(cmdBufs.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::BufferImageCopy region{
        .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .layerCount = 1},
        .imageExtent = {w, h, 1}};
    cmd.copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal,
                          region);

    cmd.end();
    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*cmd};
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
