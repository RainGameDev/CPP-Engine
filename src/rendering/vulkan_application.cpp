#include "rendering/vulkan_application.h"
#include "assets/material_loader.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "glm/ext/vector_float3.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <memory>

void VulkanApplication::run() {
  initVulkan();
  mainLoop();
  cleanup();
}

void VulkanApplication::initVulkan() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

  firstMouse = true;

  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, mouse_callback);

  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  assetManager.addLoader(std::make_unique<ShaderLoader>(device));
  assetManager.loadDirectory("assets/shaders");
  createSwapChain();
  createImageViews();
  createCommandPool();
  createDescriptorSetLayout();
  assetManager.addLoader(std::make_unique<TextureLoader>(
      device, physicalDevice, commandPool, queue, queueIndex));
  assetManager.loadDirectory("assets/textures");

  createDescriptorPool();
  assetManager.addLoader(std::make_unique<MaterialLoader>(
      device, physicalDevice, materialSetLayout, descriptorPool,
      commandPool, queue, assetManager));
  assetManager.loadDirectory("assets/materials");

  createUniformBuffers();
  createDescriptorSets();
  createGraphicsPipelineLayout();

  createCubeMesh();
  createCommandBuffer();
  createSyncObjects();

  initImGui();
}

void VulkanApplication::mainLoop() {
  float lastTime = 0.0f;
  while (!glfwWindowShouldClose(window)) {
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    glfwPollEvents();
    processInput(window, camera, deltaTime);

    updateUniformBuffer(currentFrame);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("Debug", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    glm::vec3 position = camera.getPosition();
    ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y,
                position.z);
    ImGui::End();

    ImGui::Render();

    drawFrame();
  }
}

void VulkanApplication::cleanup() {
  cleanupImGui();
  glfwDestroyWindow(window);
  glfwTerminate();
}

void VulkanApplication::processInput(GLFWwindow *window, Camera &camera,
                                     float deltaTime) {
  glm::vec3 inputDir = glm::vec3(0.0);

  // glfwGetMouse

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    inputDir.z += 1.0;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    inputDir.z -= 1.0;

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    inputDir.x -= 1.0;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    inputDir.x += 1.0;

  camera.processKeyboard(inputDir, deltaTime);
}

double VulkanApplication::lastX = 0.0;
double VulkanApplication::lastY = 0.0;
bool VulkanApplication::firstMouse = true;

void VulkanApplication::mouse_callback(GLFWwindow *window, double xpos,
                                       double ypos) {
  auto *app =
      static_cast<VulkanApplication *>(glfwGetWindowUserPointer(window));

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  double deltaX = xpos - lastX;
  double deltaY = lastY - ypos;
  lastX = xpos;
  lastY = ypos;

  app->camera.processMouseMovement(deltaX, deltaY);
}

uint32_t VulkanApplication::findMemoryType(uint32_t typeFilter,
                                           vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    bool typeSupported = typeFilter & (1 << i);
    bool propertiesSupported =
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

    if (typeSupported && propertiesSupported) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanApplication::createDescriptorPool() {
  constexpr uint32_t maxMaterials = 64;

  std::array<vk::DescriptorPoolSize, 2> poolSizes = {
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                             maxConcurrentFrames},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                             maxMaterials * 4}};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames + maxMaterials,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
