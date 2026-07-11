#pragma once

#include "rendering/camera.h"
#include "rendering/vulkan_render_context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application {
public:
  bool framebufferResized = false;
  void run();

private:
  GLFWwindow *window = nullptr;
  VulkanRenderingContext renderer;
  Camera camera;

  static double lastX, lastY;
  static bool firstMouse;

  void mainLoop();
  void cleanup();

  void processInput(float deltaTime);
  static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
};

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                       int height) {
  auto app = static_cast<Application *>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}
