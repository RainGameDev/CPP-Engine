#pragma once

#include "ecs/entity.h"
#include "ecs/schedule.h"
#include "ecs/system_registry.h"
#include "ecs/systems.h"
#include "ecs/world.h"
#include "rendering/camera.h"
#include "rendering/vulkan_render_context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application {
public:
  bool framebufferResized = false;
  void run();

  Application() { SystemRegistry::instance().apply_to(schedule); }

  template <typename F> Application &add_startup_system(F system) {
    schedule.add_system(Stage::Start, std::move(system));
    return *this;
  }

  template <typename F> Application &add_system(F system) {
    schedule.add_system(Stage::Update, std::move(system));
    return *this;
  }

  template <typename F> Application &add_fixed_system(F system) {
    schedule.add_system(Stage::FixedUpdate, std::move(system));
    return *this;
  }

private:
  GLFWwindow *window = nullptr;
  VulkanRenderingContext renderer;
  World world;

  Schedule schedule;
  EntityId cameraEntity;
  EntityId lightEntity;
  EntityId debugIndicatorEntity;

  static double lastX, lastY;
  static bool firstMouse;
  bool mouseCaptured = true;

  float fixedTimeStep = 1.0f / 60.0f; // the size of one physics step
  float fixedAccumulator = 0.0f;

  void mainLoop();
  void cleanup();

  void tick(float deltaTime);

  void processInput(float deltaTime);
  static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
};

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  auto app = static_cast<Application *>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}
