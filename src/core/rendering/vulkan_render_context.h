#pragma once

#include "assets/shader_loader.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "mesh.h"
#include "ubo.h"

#include "ecs/world.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 800;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class VulkanRenderingContext {
public:
  bool framebufferResized = false;

  void init(GLFWwindow *window, World &ecsWorld);
  void cleanup();
  void drawFrame();
  void updateUniformBuffer(uint32_t frame, const UniformBufferObject &ubo);
  void updateUniformBuffer(uint32_t frame);

  UniformBufferObject lastUbo{};

  GLFWwindow *window = nullptr;

  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  vk::PhysicalDeviceFeatures deviceFeatures;
  vk::raii::Queue graphicsQueue = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::Queue queue = nullptr;
  uint32_t queueIndex = ~0;

  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  vk::SurfaceFormatKHR swapChainSurfaceFormat;
  vk::Extent2D swapChainExtent;
  std::vector<vk::raii::ImageView> swapChainImageViews;

  vk::raii::Image depthImage{nullptr};
  vk::raii::DeviceMemory depthImageMemory{nullptr};
  vk::raii::ImageView depthImageView{nullptr};
  vk::Format depthFormat = vk::Format::eD32Sfloat;

  vk::raii::PipelineLayout pipelineLayout = nullptr;

  std::unordered_map<ShaderKey, std::unique_ptr<vk::raii::Pipeline>,
                     ShaderKeyHash, ShaderKeyEqual>
      graphicsPipelines;

  vk::raii::CommandPool commandPool = nullptr;
  vk::raii::CommandBuffer commandBuffer = nullptr;
  vk::raii::Semaphore presentCompleteSemaphore = nullptr;
  vk::raii::Semaphore renderFinishedSemaphore = nullptr;
  vk::raii::Fence drawFence = nullptr;
  std::vector<const char *> requiredDeviceExtension = {
      vk::KHRSwapchainExtensionName};

  std::array<UboBuffer, maxConcurrentFrames> uniformBuffers;
  std::array<UboBuffer, maxConcurrentFrames> transformBuffers;
  std::array<vk::DeviceSize, maxConcurrentFrames> transformBufferSizes{};
  std::array<UboBuffer, maxConcurrentFrames> lightBuffers;

  vk::raii::DescriptorPool descriptorPool = nullptr;
  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::DescriptorSetLayout materialSetLayout = nullptr;
  vk::raii::DescriptorSets descriptorSets = nullptr;

  uint32_t currentFrame = 0;

  World *world = nullptr;

  // Vulkan setup
  void createInstance();
  std::vector<const char *> getRequiredInstanceExtensions();
  void setupDebugMessenger();
  bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice);
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();
  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);

  uint32_t chooseSwapMinImageCount(
      vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
  vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);
  vk::PresentModeKHR chooseSwapPresentMode(
      std::vector<vk::PresentModeKHR> const &availablePresentModes);
  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats);
  void createImageViews();
  void createDepthResources();
  vk::Format findSupportedDepthFormat();
  void createCommandPool();
  void createCommandBuffer();
  void recordCommandBuffer(uint32_t imageIndex, uint32_t currentFrame);
  void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask,
                               vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask);

  // Graphics Pipelines
  void createGraphicsPipelineLayout();
  vk::Pipeline getOrCreatePipeline(const ShaderKey &key);
  void createPipelineForKey(const ShaderKey &key);

  // Buffers
  void createUniformBuffers();
  void createDescriptorSetLayout();
  void createDescriptorSets();
  void createDescriptorPool();

  void createCubeMesh();
  std::shared_ptr<Mesh> createIndicatorMesh();
  void ensureTransformBuffer(uint32_t frame, uint32_t meshCount);
  void updateLightBuffer(uint32_t frame);
  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size);

  void createSyncObjects();

  void initImGui();
  void cleanupImGui();

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *);
};
