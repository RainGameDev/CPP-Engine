#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class InputDevice : uint32_t { Keyboard, Mouse };

struct Keybinding {
  uint32_t key;
  uint32_t inputType;
  InputDevice device = InputDevice::Keyboard;

  bool operator==(const Keybinding &other) const {
    return key == other.key && inputType == other.inputType &&
           device == other.device;
  }
  size_t operator()(const Keybinding &k) const {
    auto h1 = std::hash<uint32_t>{}(k.key);
    auto h2 = std::hash<uint32_t>{}(k.inputType);
    auto h3 = std::hash<uint32_t>{}(static_cast<uint32_t>(k.device));
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};
struct InputManager {
public:
  GLFWwindow *window;

  std::vector<uint32_t> keysPressed, keysReleased, keysHeld;
  std::vector<uint32_t> mousePressed, mouseReleased, mouseHeld;

  std::unordered_map<std::string, Keybinding> keybindings;
  std::unordered_map<uint32_t, bool> prevKeyState;
  std::unordered_map<uint32_t, bool> prevMouseState;

  double mouseX = 0.0, mouseY = 0.0;
  double prevMouseX = 0.0, prevMouseY = 0.0;
  double mouseDeltaX = 0.0, mouseDeltaY = 0.0;
  double scrollX = 0.0, scrollY = 0.0;
  bool firstMouse = true;

  /// Binds a keybind
  void addKeybind(Keybinding keybind, std::string name) {
    keybindings.insert(std::make_pair(name, keybind));
    if (keybind.device == InputDevice::Keyboard)
      prevKeyState.try_emplace(keybind.key, false);
    else
      prevMouseState.try_emplace(keybind.key, false);
  }

  /// Rebinds a keybind if it exists, adds it if it doesnt.
  void updateKeybind(Keybinding keybind, std::string name) {
    if (keybindings.contains(name)) {
      keybindings[name] = keybind;
    } else {
      addKeybind(keybind, name);
      return;
    }
    if (keybind.device == InputDevice::Keyboard)
      prevKeyState.try_emplace(keybind.key, false);
    else
      prevMouseState.try_emplace(keybind.key, false);
  }

  void update() {
    keysPressed.clear();
    keysReleased.clear();
    keysHeld.clear();
    mousePressed.clear();
    mouseReleased.clear();
    mouseHeld.clear();

    for (auto &[key, wasDown] : prevKeyState) {
      bool isDown = glfwGetKey(window, key) == GLFW_PRESS;
      if (isDown && !wasDown)
        keysPressed.push_back(key);
      else if (isDown && wasDown)
        keysHeld.push_back(key);
      else if (!isDown && wasDown)
        keysReleased.push_back(key);
      wasDown = isDown;
    }

    for (auto &[button, wasDown] : prevMouseState) {
      bool isDown = glfwGetMouseButton(window, button) == GLFW_PRESS;
      if (isDown && !wasDown)
        mousePressed.push_back(button);
      else if (isDown && wasDown)
        mouseHeld.push_back(button);
      else if (!isDown && wasDown)
        mouseReleased.push_back(button);
      wasDown = isDown;
    }

    glfwGetCursorPos(window, &mouseX, &mouseY);
    if (firstMouse) {
      prevMouseX = mouseX;
      prevMouseY = mouseY;
      firstMouse = false;
    }
    mouseDeltaX = mouseX - prevMouseX;
    mouseDeltaY = prevMouseY - mouseY;
    prevMouseX = mouseX;
    prevMouseY = mouseY;
  }

  /// Is keybind[name] active?
  bool isKeybindActive(std::string name) {
    if (!keybindings.contains(name))
      return false;
    Keybinding bind = keybindings[name];

    auto &pressedList =
        (bind.device == InputDevice::Keyboard) ? keysPressed : mousePressed;
    auto &heldList =
        (bind.device == InputDevice::Keyboard) ? keysHeld : mouseHeld;
    auto &releasedList =
        (bind.device == InputDevice::Keyboard) ? keysReleased : mouseReleased;

    switch (bind.inputType) {
    case GLFW_PRESS:
      return std::find(pressedList.begin(), pressedList.end(), bind.key) !=
             pressedList.end();
    case GLFW_REPEAT:
      return std::find(heldList.begin(), heldList.end(), bind.key) !=
             heldList.end();
    case GLFW_RELEASE:
      return std::find(releasedList.begin(), releasedList.end(), bind.key) !=
             releasedList.end();
    }
    return false;
  }

  glm::vec2 inputVec2(std::string xPos, std::string xNeg, std::string yPos,
                      std::string yNeg) {

    glm::vec2 input = glm::vec2(0, 0);

    if (isKeybindActive(xPos)) {
      input.x = 1.0;
    } else if (isKeybindActive(xNeg)) {
      input.x = -1.0;
    } else {
      input.x = 0.0;
    }

    if (isKeybindActive(yPos)) {
      input.y = 1.0;
    } else if (isKeybindActive(yNeg)) {
      input.y = -1.0;
    } else {
      input.y = 0.0;
    }

    return input;
  }

  glm::vec3 inputVec3(std::string xPos, std::string xNeg, std::string yPos,
                      std::string yNeg, std::string zPos, std::string zNeg) {

    glm::vec3 input = glm::vec3(0, 0, 0);

    if (isKeybindActive(xPos)) {
      input.x = 1.0;
    } else if (isKeybindActive(xNeg)) {
      input.x = -1.0;
    } else {
      input.x = 0.0;
    }

    if (isKeybindActive(yPos)) {
      input.y = 1.0;
    } else if (isKeybindActive(yNeg)) {
      input.y = -1.0;
    } else {
      input.y = 0.0;
    }

    if (isKeybindActive(zPos)) {
      input.z = 1.0;
    } else if (isKeybindActive(zNeg)) {
      input.z = -1.0;
    } else {
      input.z = 0.0;
    }

    return input;
  }
};
