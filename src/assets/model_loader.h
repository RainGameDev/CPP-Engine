#pragma once
#include "asset_loader.h"
#include "rendering/mesh.h"
#include <fstream>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct ShaderAsset {
  Mesh mesh;
};
