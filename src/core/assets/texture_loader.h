#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "asset_loader.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.hpp"
#include <stb_image.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

// DDS file structures
#pragma pack(push, 1)
struct DDSHeader {
  uint32_t magic;      // "DDS" = 0x20534444
  uint32_t headerSize; // 124
  uint32_t flags;
  uint32_t height;
  uint32_t width;
  uint32_t pitchOrLinearSize;
  uint32_t depth;
  uint32_t mipMapCount;
  uint32_t reserved1[11];
  struct {
    uint32_t dwSize; // 32
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
  } ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

struct DDSHeaderDX10 {
  uint32_t dxgiFormat;
  uint32_t resourceDimension;
  uint32_t miscFlag;
  uint32_t arraySize;
  uint32_t miscFlags2;
};
#pragma pack(pop)

enum {
  DDS_FOURCC = 0x4,
  DDS_RGB = 0x40,
  DDS_LUMINANCE = 0x20000,
  DDS_ALPHA = 0x1,
};

// DXGI formats
enum DXGIFormat : uint32_t {
  DXGI_FORMAT_UNKNOWN = 0,
  DXGI_FORMAT_R8_UNORM = 61,
  DXGI_FORMAT_BC1_TYPELESS = 70,
  DXGI_FORMAT_BC1_UNORM = 71,
  DXGI_FORMAT_BC1_UNORM_SRGB = 72,
  DXGI_FORMAT_BC2_TYPELESS = 73,
  DXGI_FORMAT_BC2_UNORM = 74,
  DXGI_FORMAT_BC2_UNORM_SRGB = 75,
  DXGI_FORMAT_BC3_TYPELESS = 76,
  DXGI_FORMAT_BC3_UNORM = 77,
  DXGI_FORMAT_BC3_UNORM_SRGB = 78,
  DXGI_FORMAT_BC4_TYPELESS = 79,
  DXGI_FORMAT_BC4_UNORM = 80,
  DXGI_FORMAT_BC4_SNORM = 81,
  DXGI_FORMAT_BC5_TYPELESS = 82,
  DXGI_FORMAT_BC5_UNORM = 83,
  DXGI_FORMAT_BC5_SNORM = 84,
  DXGI_FORMAT_BC6H_TYPELESS = 94,
  DXGI_FORMAT_BC6H_UF16 = 95,
  DXGI_FORMAT_BC6H_SF16 = 96,
  DXGI_FORMAT_BC7_TYPELESS = 97,
  DXGI_FORMAT_BC7_UNORM = 98,
  DXGI_FORMAT_BC7_UNORM_SRGB = 99,
};

struct TextureAsset {
  uint32_t width, height, mipLevels;
  vk::Format format;
  vk::raii::Image image = nullptr;
  vk::raii::ImageView imageView = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Sampler sampler = nullptr;
  VkDescriptorSet imguiDS = VK_NULL_HANDLE;
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

  static bool isDDS(const std::string &path) {
    return path.size() >= 4 && path.compare(path.size() - 4, 4, ".dds") == 0;
  }

  static vk::Format ddsToVulkanFormat(uint32_t dxgiFormat) {
    switch (dxgiFormat) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_TYPELESS:
      return vk::Format::eBc1RgbaUnormBlock;
    case DXGI_FORMAT_BC1_UNORM_SRGB:
      return vk::Format::eBc1RgbaSrgbBlock;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
      return vk::Format::eBc2UnormBlock;
    case DXGI_FORMAT_BC2_UNORM_SRGB:
      return vk::Format::eBc2SrgbBlock;
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_TYPELESS:
      return vk::Format::eBc3UnormBlock;
    case DXGI_FORMAT_BC3_UNORM_SRGB:
      return vk::Format::eBc3SrgbBlock;
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_TYPELESS:
      return vk::Format::eBc4UnormBlock;
    case DXGI_FORMAT_BC4_SNORM:
      return vk::Format::eBc4SnormBlock;
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_TYPELESS:
      return vk::Format::eBc5UnormBlock;
    case DXGI_FORMAT_BC5_SNORM:
      return vk::Format::eBc5SnormBlock;
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_TYPELESS:
      return vk::Format::eBc6HUfloatBlock;
    case DXGI_FORMAT_BC6H_SF16:
      return vk::Format::eBc6HSfloatBlock;
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_TYPELESS:
      return vk::Format::eBc7UnormBlock;
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return vk::Format::eBc7SrgbBlock;
    case DXGI_FORMAT_R8_UNORM:
      return vk::Format::eR8Unorm;
    default:
      return vk::Format::eUndefined;
    }
  }

  static uint32_t getBlockSize(vk::Format fmt) {
    switch (fmt) {
    case vk::Format::eBc1RgbaUnormBlock:
    case vk::Format::eBc1RgbaSrgbBlock:
    case vk::Format::eBc4UnormBlock:
    case vk::Format::eBc4SnormBlock:
      return 8; // 8 bytes per 4x4 block
    default:
      return 16; // 16 bytes per 4x4 block
    }
  }

  static uint32_t align(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
  }

  /// Evil ass math function that fixes my mipmaps
  static std::vector<std::vector<uint8_t>>
  generateMipChainSRGB(const uint8_t *basePixels, int w, int h,
                       uint32_t mipLevels) {
    // Convert the bytes to be linear
    auto srgbToLinear = [](uint8_t c) {
      float v = c / 255.0f;
      return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    };

    // Convert the linear back to SRGB
    auto linearToSrgb = [](float v) {
      v = std::clamp(v, 0.0f, 1.0f);
      float c = v <= 0.0031308f ? v * 12.92f
                                : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
      return static_cast<uint8_t>(std::round(c * 255.0f));
    };

    std::vector<std::vector<uint8_t>> mips;
    mips.emplace_back(basePixels,
                      // Mip 0 is the original image
                      basePixels + (size_t)w * h * 4);

    int curW = w, curH = h;
    for (uint32_t level = 1; level < mipLevels; level++) {
      // each mip is half the size of the previous one
      int nextW = std::max(1, curW / 2);
      int nextH = std::max(1, curH / 2);

      // source data = previous mip level
      const uint8_t *prev = mips.back().data();
      std::vector<uint8_t> next((size_t)nextW * nextH * 4);

      for (int y = 0; y < nextH; y++) {
        for (int x = 0; x < nextW; x++) {
          // 2x2 block of source pixels to scale
          // clamped so its not read past the edge
          int x0 = x * 2, x1 = std::min(x * 2 + 1, curW - 1);
          int y0 = y * 2, y1 = std::min(y * 2 + 1, curH - 1);

          // loop over R, G, B, A channels
          for (int c = 0; c < 4; c++) {
            // fetch one channel of one source pixel, converting sRGB tolinear
            auto sample = [&](int sx, int sy) {
              uint8_t v = prev[((size_t)sy * curW + sx) * 4 + c];
              return c == 3 ? v / 255.0f : srgbToLinear(v);
            };

            // average the 2x2 block in linear space
            float avg = (sample(x0, y0) + sample(x1, y0) + sample(x0, y1) +
                         sample(x1, y1)) /
                        4.0f;

            // write result back, converting linear to sRGB for color channels,
            next[((size_t)y * nextW + x) * 4 + c] =
                c == 3 ? static_cast<uint8_t>(std::round(avg * 255.0f))
                       : linearToSrgb(avg);
          }
        }
      }

      // store this level, move on to the next
      mips.push_back(std::move(next));
      curW = nextW;
      curH = nextH;
    }
    return mips;
  }

  TextureAsset loadAsset(const std::string &path) override {

    if (isDDS(path)) {
      return loadDDS(path);
    }

    // Load pixels from the file
    int w, h, channels;
    stbi_uc *pixels =
        stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels)
      throw std::runtime_error("Failed to load texture: " + path);

    uint32_t mipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

    auto mipChain = generateMipChainSRGB(pixels, w, h, mipLevels);
    free(pixels);

    vk::DeviceSize totalSize = 0;
    std::vector<vk::DeviceSize> offsets(mipLevels);
    for (uint32_t i = 0; i < mipLevels; i++) {
      offsets[i] = totalSize;
      totalSize += mipChain[i].size();
    }

    // Create the staging buffer
    vk::raii::Buffer stagingBuffer{nullptr};
    vk::raii::DeviceMemory stagingMemory{nullptr};
    createBuffer(totalSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingMemory);

    void *mapped = stagingMemory.mapMemory(0, totalSize);
    for (uint32_t i = 0; i < mipLevels; i++)
      memcpy(static_cast<uint8_t *>(mapped) + offsets[i], mipChain[i].data(),
             mipChain[i].size());
    stagingMemory.unmapMemory();

    /// Create VKImage
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory imageMemory{nullptr};
    createImage(
        w, h, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        image, imageMemory);

    transitionImageLayout(image, mipLevels, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);

    // Create command buffers
    auto commandBuffers = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(commandBuffers.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Create mipmaps
    int mipW = w, mipH = h;
    for (uint32_t i = 0; i < mipLevels; i++) {
      vk::BufferImageCopy region{
          .bufferOffset = offsets[i],
          .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .mipLevel = i,
                               .layerCount = 1},
          .imageExtent = {static_cast<uint32_t>(mipW),
                          static_cast<uint32_t>(mipH), 1}};
      cmd.copyBufferToImage(*stagingBuffer, *image,
                            vk::ImageLayout::eTransferDstOptimal, region);
      mipW = std::max(1, mipW / 2);
      mipH = std::max(1, mipH / 2);
    }
    cmd.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*cmd};
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();

    transitionImageLayout(image, mipLevels,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    // Create samplers
    vk::raii::ImageView imageView =
        createImageView(image, vk::Format::eR8G8B8A8Srgb);
    vk::raii::Sampler sampler = createSampler(mipLevels);

    auto imguiDS = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(*sampler), static_cast<VkImageView>(*imageView),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return {static_cast<uint32_t>(w),
            static_cast<uint32_t>(h),
            mipLevels,
            vk::Format::eR8G8B8A8Srgb,
            std::move(image),
            std::move(imageView),
            std::move(imageMemory),
            std::move(sampler),
            std::move(imguiDS)

    };
  }

private:
  TextureAsset loadDDS(const std::string &path) {
    // Read entire file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      throw std::runtime_error("Failed to open DDS file: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char *>(fileData.data()), fileSize);
    file.close();

    // Parse header
    if (fileSize < sizeof(DDSHeader))
      throw std::runtime_error("DDS file too small: " + path);

    DDSHeader header;
    memcpy(&header, fileData.data(), sizeof(DDSHeader));

    if (header.magic != 0x20534444) // "DDS "
      throw std::runtime_error("Invalid DDS magic number: " + path);

    uint32_t width = header.width;
    uint32_t height = header.height;
    uint32_t mipLevels = header.mipMapCount;
    if (mipLevels == 0)
      mipLevels = 1;

    // Determine format
    vk::Format format = vk::Format::eUndefined;
    bool hasDX10Header = (header.ddspf.dwFourCC == 0x30315844); // "DX10"

    if (hasDX10Header) {
      if (fileSize < sizeof(DDSHeader) + sizeof(DDSHeaderDX10))
        throw std::runtime_error("DDS DX10 header too small: " + path);

      DDSHeaderDX10 dx10Header;
      memcpy(&dx10Header, fileData.data() + sizeof(DDSHeader),
             sizeof(DDSHeaderDX10));
      format = ddsToVulkanFormat(dx10Header.dxgiFormat);
    } else {
      // Legacy FourCC formats
      uint32_t fourCC = header.ddspf.dwFourCC;
      switch (fourCC) {
      case 0x31545844: // "DXT1"
        format = vk::Format::eBc1RgbaUnormBlock;
        break;
      case 0x33545844: // "DXT3"
        format = vk::Format::eBc2UnormBlock;
        break;
      case 0x35545844: // "DXT5"
        format = vk::Format::eBc3UnormBlock;
        break;
      case 0x55344342: // "BC4U"
        format = vk::Format::eBc4UnormBlock;
        break;
      case 0x53344342: // "BC4S"
        format = vk::Format::eBc4SnormBlock;
        break;
      case 0x55354342: // "BC5U"
        format = vk::Format::eBc5UnormBlock;
        break;
      case 0x53354342: // "BC5S"
        format = vk::Format::eBc5SnormBlock;
        break;
      default:
        // Try uncompressed formats from bit masks
        if (header.ddspf.dwFlags & DDS_RGB) {
          if (header.ddspf.dwRGBBitCount == 32) {
            if (header.ddspf.dwRBitMask == 0x000000FF &&
                header.ddspf.dwGBitMask == 0x0000FF00 &&
                header.ddspf.dwBBitMask == 0x00FF0000) {
              format = vk::Format::eR8G8B8A8Unorm;
            }
          } else if (header.ddspf.dwRGBBitCount == 24) {
            format = vk::Format::eR8G8B8Unorm;
          }
        } else if (header.ddspf.dwFlags & DDS_LUMINANCE) {
          if (header.ddspf.dwRGBBitCount == 8) {
            format = vk::Format::eR8Unorm;
          }
        }
        break;
      }
    }

    if (format == vk::Format::eUndefined)
      throw std::runtime_error("Unsupported DDS format: " + path);

    // Get block size for compressed formats
    bool isCompressed = (format >= vk::Format::eBc1RgbaUnormBlock &&
                         format <= vk::Format::eBc7SrgbBlock);
    uint32_t blockSize = isCompressed ? getBlockSize(format) : 4;
    uint32_t blockAlignment = isCompressed ? 4 : 1;

    // Calculate mip sizes and total size
    vk::DeviceSize totalSize = 0;
    std::vector<vk::DeviceSize> offsets(mipLevels);
    std::vector<uint32_t> mipWidths(mipLevels);
    std::vector<uint32_t> mipHeights(mipLevels);

    for (uint32_t i = 0; i < mipLevels; i++) {
      uint32_t mipW = std::max(1u, width >> i);
      uint32_t mipH = std::max(1u, height >> i);
      mipWidths[i] = mipW;
      mipHeights[i] = mipH;

      uint32_t alignedW = align(mipW, blockAlignment);
      uint32_t alignedH = align(mipH, blockAlignment);

      vk::DeviceSize levelSize;
      if (isCompressed) {
        levelSize = (alignedW / 4) * (alignedH / 4) * blockSize;
      } else {
        levelSize = alignedW * alignedH * blockSize;
      }

      offsets[i] = totalSize;
      totalSize += levelSize;
    }

    // Determine data offset (after headers)
    size_t dataOffset = sizeof(DDSHeader);
    if (hasDX10Header)
      dataOffset += sizeof(DDSHeaderDX10);

    if (dataOffset + totalSize > fileSize)
      throw std::runtime_error("DDS file data truncated: " + path);

    // Create staging buffer
    vk::raii::Buffer stagingBuffer{nullptr};
    vk::raii::DeviceMemory stagingMemory{nullptr};
    createBuffer(totalSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingMemory);

    void *mapped = stagingMemory.mapMemory(0, totalSize);
    memcpy(mapped, fileData.data() + dataOffset, totalSize);
    stagingMemory.unmapMemory();

    // Create image
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory imageMemory{nullptr};
    createImage(width, height, mipLevels, format, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eTransferDst,
                image, imageMemory);

    transitionImageLayout(image, mipLevels, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);

    // Create command buffer and copy mip levels
    auto commandBuffers = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(commandBuffers.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    for (uint32_t i = 0; i < mipLevels; i++) {
      vk::BufferImageCopy region{
          .bufferOffset = offsets[i],
          .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .mipLevel = i,
                               .layerCount = 1},
          .imageExtent = {mipWidths[i], mipHeights[i], 1}};
      cmd.copyBufferToImage(*stagingBuffer, *image,
                            vk::ImageLayout::eTransferDstOptimal, region);
    }
    cmd.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*cmd};
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();

    transitionImageLayout(image, mipLevels,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    // Create image view and sampler
    vk::raii::ImageView imageView = createImageView(image, format);
    vk::raii::Sampler sampler = createSampler(mipLevels);

    auto imguiDS = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(*sampler), static_cast<VkImageView>(*imageView),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return {width,
            height,
            mipLevels,
            format,
            std::move(image),
            std::move(imageView),
            std::move(imageMemory),
            std::move(sampler),

            std::move(imguiDS)};
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

  void transitionImageLayoutSingle(vk::raii::CommandBuffer &cmd,
                                   vk::raii::Image &image, uint32_t mipLevel,
                                   vk::ImageLayout oldLayout,
                                   vk::ImageLayout newLayout) {
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
                             .baseMipLevel = mipLevel,
                             .levelCount = 1,
                             .layerCount = 1}};
    vk::DependencyInfo depInfo{.imageMemoryBarrierCount = 1,
                               .pImageMemoryBarriers = &barrier};
    cmd.pipelineBarrier2(depInfo);
  }

  void generateMipmaps(vk::raii::Image &image, int32_t w, int32_t h,
                       uint32_t mipLevels) {
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

    transitionImageLayoutSingle(cmd, image, 0,
                                vk::ImageLayout::eTransferDstOptimal,
                                vk::ImageLayout::eTransferSrcOptimal);

    for (uint32_t i = 1; i < mipLevels; i++) {
      int32_t nextW = mipW > 1 ? mipW / 2 : 1;
      int32_t nextH = mipH > 1 ? mipH / 2 : 1;

      vk::ImageBlit blit{};
      blit.srcOffsets[0] = {0, 0, 0};
      blit.srcOffsets[1] = {mipW, mipH, 1};
      blit.srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = i - 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1};
      blit.dstOffsets[0] = {0, 0, 0};
      blit.dstOffsets[1] = {nextW, nextH, 1};
      blit.dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = i,
                             .baseArrayLayer = 0,
                             .layerCount = 1};

      cmd.blitImage(*image, vk::ImageLayout::eTransferSrcOptimal, *image,
                    vk::ImageLayout::eTransferDstOptimal, blit,
                    vk::Filter::eLinear);

      transitionImageLayoutSingle(cmd, image, i - 1,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::ImageLayout::eShaderReadOnlyOptimal);

      if (i < mipLevels - 1) {
        transitionImageLayoutSingle(cmd, image, i,
                                    vk::ImageLayout::eTransferDstOptimal,
                                    vk::ImageLayout::eTransferSrcOptimal);
      }

      mipW = nextW;
      mipH = nextH;
    }

    transitionImageLayoutSingle(cmd, image, mipLevels - 1,
                                vk::ImageLayout::eTransferDstOptimal,
                                vk::ImageLayout::eShaderReadOnlyOptimal);

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
                              .anisotropyEnable = vk::True,
                              .maxAnisotropy = limits.maxSamplerAnisotropy,
                              .maxLod = static_cast<float>(mipLevels - 1)});
  }
};
