#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "asset_loader.h"
#include "vulkan/vulkan.hpp"
#include <stb_image.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct TextureAsset {
  uint32_t width, height, mipLevels;
  vk::Format format;
  vk::raii::Image image = nullptr;
  vk::raii::ImageView imageView = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Sampler sampler = nullptr;
};

class TextureLoader : public AssetLoader<TextureAsset> {
  vk::raii::Device &device;
  vk::raii::PhysicalDevice &physicalDevice;
  vk::raii::CommandPool &commandPool;
  vk::raii::Queue &queue;
  uint32_t queueIndex;

public:
  TextureLoader(vk::raii::Device &device,
                vk::raii::PhysicalDevice &physicalDevice,
                vk::raii::CommandPool &commandPool, vk::raii::Queue &queue,
                uint32_t queueIndex)
      : device(device), physicalDevice(physicalDevice),
        commandPool(commandPool), queue(queue), queueIndex(queueIndex) {}

  std::vector<std::string> extensions() const override {
    return {".png", ".jpg", ".jpeg", ".dds", ".bmp"};
  }

  TextureAsset loadAsset(const std::string &path) override {

    // Load pixels from the file
    int w, h, channels;

    stbi_uc *pixels =
        stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels)
      throw std::runtime_error("Failed to load texture: " + path);

    uint32_t mipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
    vk::DeviceSize imageSize = w * h * 4;

    // Create the staging buffer
    vk::raii::Buffer stagingBuffer{nullptr};
    vk::raii::DeviceMemory stagingMemory{nullptr};

    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingMemory);

    void *mapped = stagingMemory.mapMemory(0, imageSize);
    memcpy(mapped, pixels, imageSize);
    stagingMemory.unmapMemory();
    free(pixels);

    // Create VKImage
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory imageMemory{nullptr};
    createImage(
        w, h, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        image, imageMemory);

    // Create mipmaps
    transitionImageLayout(image, mipLevels, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, image, w, h);
    generateMipmaps(image, w, h, mipLevels);

    /// crete ImageView and Sampler
    vk::raii::ImageView imageView =
        createImageView(image, vk::Format::eR8G8B8A8Srgb);
    vk::raii::Sampler sampler = createSampler(mipLevels);

    return {static_cast<uint32_t>(w),
            static_cast<uint32_t>(h),
            mipLevels,
            vk::Format::eR8G8B8A8Srgb,
            std::move(image),
            std::move(imageView),
            std::move(imageMemory),
            std::move(sampler)};
  }

private:
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

  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory) {
    vk::BufferCreateInfo bufferInfo{.size = size,
                                    .usage = usage,
                                    .sharingMode = vk::SharingMode::eExclusive};
    buffer = vk::raii::Buffer(device, bufferInfo);

    auto memReq = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties)};
    bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
  }

  void createImage(uint32_t w, uint32_t h, uint32_t mipLevels,
                   vk::Format format, vk::ImageTiling tiling,
                   vk::ImageUsageFlags usage, vk::raii::Image &image,
                   vk::raii::DeviceMemory &imageMemory) {
    vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                  .format = format,
                                  .extent = {w, h, 1},
                                  .mipLevels = mipLevels,
                                  .arrayLayers = 1,
                                  .samples = vk::SampleCountFlagBits::e1,
                                  .tiling = tiling,
                                  .usage = usage,
                                  .sharingMode = vk::SharingMode::eExclusive,
                                  .initialLayout = vk::ImageLayout::eUndefined};
    image = vk::raii::Image(device, imageInfo);

    auto memReq = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(
            memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
    imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(*imageMemory, 0);
  }

  void copyBufferToImage(vk::raii::Buffer &buffer, vk::raii::Image &image,
                         uint32_t w, uint32_t h) {
    auto commandBuffers = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(commandBuffers.front());
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

  void transitionImageLayout(vk::raii::Image &image, uint32_t mipLevels,
                             vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout) {
    auto commandBuffers = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(commandBuffers.front());
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
                             .levelCount = mipLevels,
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

  void generateMipmaps(vk::raii::Image &image, int32_t w, int32_t h,
                       uint32_t mipLevels) {
    // Check if linear blitting is supported for this format
    auto formatProps =
        physicalDevice.getFormatProperties(vk::Format::eR8G8B8A8Srgb);
    if (!(formatProps.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
      throw std::runtime_error(
          "texture format does not support linear blitting");

    auto commandBuffers = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(commandBuffers.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    int32_t mipW = w, mipH = h;
    for (uint32_t i = 1; i < mipLevels; i++) {
      mipW = mipW > 1 ? mipW / 2 : 1;
      mipH = mipH > 1 ? mipH / 2 : 1;
    }

    cmd.end();
    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*cmd};
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
  }

  vk::raii::ImageView createImageView(vk::raii::Image &image,
                                      vk::Format format) {
    return vk::raii::ImageView(
        device,
        vk::ImageViewCreateInfo{
            .image = *image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                 .levelCount = VK_REMAINING_MIP_LEVELS,
                                 .layerCount = 1}});
  }

  vk::raii::Sampler createSampler(uint32_t mipLevels) {
    auto limits = physicalDevice.getProperties().limits;
    return vk::raii::Sampler(
        device,
        vk::SamplerCreateInfo{.magFilter = vk::Filter::eLinear,
                              .minFilter = vk::Filter::eLinear,
                              .mipmapMode = vk::SamplerMipmapMode::eLinear,
                              .addressModeU = vk::SamplerAddressMode::eRepeat,
                              .addressModeV = vk::SamplerAddressMode::eRepeat,
                              .maxAnisotropy = limits.maxSamplerAnisotropy,
                              .maxLod = static_cast<float>(mipLevels - 1)});
  }
};
