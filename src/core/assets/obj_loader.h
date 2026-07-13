#pragma once

#include "rendering/vertex.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ObjData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};

ObjData loadObj(const std::string &path);
