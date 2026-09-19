#pragma once

#include "glm/ext/vector_float4.hpp"
#include <glm/glm.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t MAX_SHADOWS = 4;

struct UniformBufferObject {
  glm::mat4 view;
  glm::mat4 proj;
  glm::vec4 pos;
};

struct TransformUBO {
  glm::mat4 model;
};

constexpr uint32_t MAX_LIGHTS = 16;

constexpr uint32_t maxConcurrentFrames = 2;

struct UboBuffer {
  vk::raii::Buffer buffer{nullptr};
  vk::raii::DeviceMemory memory{nullptr};
  void *mapped = nullptr;
};
struct ShadowData {
  glm::mat4
      viewProj;      // dir:ortho, spot:persp, point:per-face persp (for render)
  glm::vec4 pos_far; // xyz:pos,w:far
  glm::vec4 dir_type; // xyz:dir,w:0=dir,1=spot,2=point
};

struct ShadowUBO {
  ShadowData shadows[MAX_SHADOWS];
  uint32_t count;
  uint32_t _pad[3];
};

struct LightUBO {
  glm::vec4 positionOrDirection;
  glm::vec4 colorAndIntensity;
  glm::vec4 params;
  glm::vec4 direction;
  glm::vec4 shadowInfo;
};
struct LightsUBO {
  LightUBO lights[MAX_LIGHTS];
  uint32_t count;
  uint32_t _pad[3];
};
