#pragma once

#include "glm/ext/vector_float4.hpp"
#include <glm/glm.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct UniformBufferObject {
  glm::mat4 view;
  glm::mat4 proj;
  glm::vec4 pos;
};

struct TransformUBO {
  glm::mat4 model;
};

struct LightUBO {
  glm::vec4 positionOrDirection;
  glm::vec4 colorAndIntensity;
  glm::vec4 params;
  glm::vec4 direction;
};

constexpr uint32_t MAX_LIGHTS = 16;

struct LightsUBO {
  LightUBO lights[MAX_LIGHTS];
  uint32_t count;
};

constexpr uint32_t maxConcurrentFrames = 2;

struct UboBuffer {
  vk::raii::Buffer buffer{nullptr};
  vk::raii::DeviceMemory memory{nullptr};
  void *mapped = nullptr;
};
