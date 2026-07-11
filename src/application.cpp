#include "application.h"
#include "assets/material_loader.h"
#include "glm/ext/vector_float3.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>

void Application::run() {
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

  renderer.init(window);

  mainLoop();
  renderer.cleanup();
  glfwDestroyWindow(window);
  glfwTerminate();
}

void Application::mainLoop() {
  float lastTime = 0.0f;
  while (!glfwWindowShouldClose(window)) {
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    glfwPollEvents();
    processInput(deltaTime);

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.getViewMatrix();
    ubo.pos = glm::vec4(camera.position, 0.0);
    ubo.proj =
        camera.getProjectionMatrix(renderer.swapChainExtent.width /
                                   (float)renderer.swapChainExtent.height);
    ubo.proj[1][1] *= -1;

    renderer.updateUniformBuffer(renderer.currentFrame, ubo);

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

    renderer.framebufferResized = framebufferResized;
    framebufferResized = false;

    ImGui::Render();

    renderer.drawFrame();
  }
}

void Application::cleanup() {}

void Application::processInput(float deltaTime) {
  glm::vec3 inputDir = glm::vec3(0.0);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    inputDir.z += 1.0;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    inputDir.z -= 1.0;

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    inputDir.y += 1.0;
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    inputDir.y -= 1.0;

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    inputDir.x -= 1.0;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    inputDir.x += 1.0;

  camera.processKeyboard(inputDir, deltaTime);
}

double Application::lastX = 0.0;
double Application::lastY = 0.0;
bool Application::firstMouse = true;

void Application::mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

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
