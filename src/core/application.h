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
#include <functional>
#include <vector>

enum class UIMode { Editor, Game, Both };

class Application {
public:
  bool framebufferResized = false;
  void run();

  Application() { SystemRegistry::instance().apply_to(schedule); }

  void setEditorMode(bool enabled) { editorMode = enabled; }

  World &getWorld() { return world; }

  void addUI(std::function<void()> ui, UIMode mode = UIMode::Both) {
    postTickUIs.push_back({std::move(ui), mode});
  }

  void addPreTickUI(std::function<void()> ui, UIMode mode = UIMode::Both) {
    preTickUIs.push_back({std::move(ui), mode});
  }

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

  bool mouseCaptured = true;
  bool editorMode = false;

  struct UICallback {
    std::function<void()> func;
    UIMode mode;
  };
  std::vector<UICallback> preTickUIs;
  std::vector<UICallback> postTickUIs;

  float fixedTimeStep = 1.0f / 60.0f; // the size of one physics step
  float fixedAccumulator = 0.0f;

  void mainLoop();
  void cleanup();

  void tick(float deltaTime);

  void processInput(float deltaTime);
};

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  auto app = static_cast<Application *>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}
