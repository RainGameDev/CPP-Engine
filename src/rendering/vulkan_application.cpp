#include "rendering/vulkan_application.h"
#include "glm/ext/vector_float3.hpp"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <iostream>
#include <print>

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
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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
  createSwapChain();
  createImageViews();
  createDescriptorSetLayout();
  createDescriptorPool();
  createUniformBuffers();
  createDescriptorSets();
  createGraphicsPipeline();
  createCommandPool();
  createCubeMesh();
  createCommandBuffer();
  createSyncObjects();
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

    drawFrame();
  }
}

void VulkanApplication::cleanup() {
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
  vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer,
                                  .descriptorCount = maxConcurrentFrames};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
