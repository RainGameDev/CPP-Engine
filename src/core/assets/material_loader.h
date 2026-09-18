#pragma once
#include "asset_loader.h"
#include "handle.h"
#include "shader_loader.h"
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
  Handle<ShaderAsset> vertexShader;
  Handle<ShaderAsset> fragmentShader;
  Handle<TextureAsset> albedo;
  Handle<TextureAsset> normal;
  Handle<TextureAsset> rmaos;
  Handle<TextureAsset> height;
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
    MaterialAsset mat = parseJson(path);
    pathByName[std::filesystem::path(path).stem().string()] = path;
    resolveAll(mat);
    allocateDescriptors(mat);
    writeDescriptors(mat);
    return mat;
  }

  void refreshDescriptors(MaterialAsset &mat) {
    resolve(assetManager, mat.vertexShader);
    resolve(assetManager, mat.fragmentShader);
    resolve(assetManager, mat.albedo);
    resolve(assetManager, mat.normal);
    resolve(assetManager, mat.rmaos);
    resolve(assetManager, mat.height);
    allocateDescriptors(mat);
    writeDescriptors(mat);
  }

  void saveAsset(const std::string &name, const MaterialAsset &mat) {
    auto it = pathByName.find(name);
    std::string path = it != pathByName.end()
                           ? it->second
                           : "assets/materials/" + name + ".json";
    nlohmann::json j;
    j["vertexShader"] = mat.vertexShader.assetName;
    j["fragmentShader"] = mat.fragmentShader.assetName;
    j["albedoMap"] = mat.albedo.assetName;
    j["normalMap"] = mat.normal.assetName;
    j["rmaosMap"] = mat.rmaos.assetName;
    j["heightMap"] = mat.height.assetName;
    j["parallaxStrength"] = mat.parallaxStrength;
    j["baseColorFactor"] = {mat.baseColorFactor.x, mat.baseColorFactor.y,
                            mat.baseColorFactor.z, mat.baseColorFactor.w};
    j["metallicFactor"] = mat.metallicFactor;
    j["roughnessFactor"] = mat.roughnessFactor;
    std::ofstream out(path);
    if (!out.is_open())
      throw std::runtime_error("failed to save material: " + path);
    out << j.dump(2);
  }

private:
  std::unordered_map<std::string, std::string> pathByName;

  MaterialAsset parseJson(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open())
      throw std::runtime_error("failed to open material: " + path);
    nlohmann::json json;
    file >> json;
    MaterialAsset mat{};
    mat.vertexShader.assetName =
        std::filesystem::path(
            json.value("vertexShader", "sdr_default_model.vert"))
            .generic_string();
    mat.fragmentShader.assetName =
        std::filesystem::path(
            json.value("fragmentShader", "sdr_default_model.frag"))
            .generic_string();
    mat.albedo.assetName =
        std::filesystem::path(json.value("albedoMap", "")).stem().string();
    mat.normal.assetName =
        std::filesystem::path(json.value("normalMap", "")).stem().string();
    mat.rmaos.assetName =
        std::filesystem::path(json.value("rmaosMap", "")).stem().string();
    mat.height.assetName =
        std::filesystem::path(json.value("heightMap", "")).stem().string();
    mat.parallaxStrength = mat.height.assetName.empty()
                               ? 0.0f
                               : json.value("parallaxStrength", 0.05f);
    mat.baseColorFactor = parseVec4(json, "baseColorFactor", {1, 1, 1, 1});
    mat.metallicFactor = json.value("metallicFactor", 1.0f);
    mat.roughnessFactor = json.value("roughnessFactor", 1.0f);
    return mat;
  }

  void resolveAll(MaterialAsset &mat) {
    resolve(assetManager, mat.vertexShader);
    resolve(assetManager, mat.fragmentShader);
    resolve(assetManager, mat.albedo);
    resolve(assetManager, mat.normal);
    resolve(assetManager, mat.rmaos);
    resolve(assetManager, mat.height);
  }

  void allocateDescriptors(MaterialAsset &mat) {
    vk::DescriptorSetLayout layouts[] = {materialLayout};
    vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = layouts};
    auto sets = vk::raii::DescriptorSets(device, allocInfo);
    mat.descriptorSet = std::move(sets.front());
  }

  void writeDescriptors(MaterialAsset &mat) {
    auto &albedo = mat.albedo ? *mat.albedo.get() : fallbackTexture;
    auto &normal = mat.normal ? *mat.normal.get() : fallbackTexture;
    auto &rmaos = mat.rmaos ? *mat.rmaos.get() : fallbackTexture;
    auto &heightTex = mat.height ? *mat.height.get() : fallbackTexture;
    auto info = [](TextureAsset &t) {
      return vk::DescriptorImageInfo{
          .sampler = *t.sampler,
          .imageView = *t.imageView,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    };
    vk::DescriptorImageInfo infos[4] = {info(albedo), info(normal), info(rmaos),
                                        info(heightTex)};
    std::array<vk::WriteDescriptorSet, 4> writes{};
    for (int i = 0; i < 4; i++)
      writes[i] = {.dstSet = *mat.descriptorSet,
                   .dstBinding = (uint32_t)i,
                   .descriptorCount = 1,
                   .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                   .pImageInfo = &infos[i]};
    device.updateDescriptorSets(writes, nullptr);
  }

  glm::vec4 parseVec4(const nlohmann::json &j, const std::string &key,
                      glm::vec4 fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() != 4)
      return fallback;
    return {j[key][0], j[key][1], j[key][2], j[key][3]};
  }

  void createFallbackTexture() {
    uint32_t pixel = 0xFFFFFFFF;
    vk::DeviceSize imageSize = sizeof(pixel);

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

    transitionImage(fallbackTexture.image, vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuf, fallbackTexture.image, 1, 1);
    transitionImage(fallbackTexture.image, vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal);

    fallbackTexture.imageView = vk::raii::ImageView(
        device,
        vk::ImageViewCreateInfo{
            .image = *fallbackTexture.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .levelCount = 1,
                                 .layerCount = 1}});

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
