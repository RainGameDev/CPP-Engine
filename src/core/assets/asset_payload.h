#pragma once
#include <cstdint>
#include <cstring>

/// Drag + drop payload for assets dragged out of the asset window
struct AssetDragPayload {
  enum Type : uint8_t { None, Material, Mesh, Shader, Texture, Scene };
  static constexpr const char *kPayloadType = "CE_ASSET";

  Type type = None;
  char name[128] = {0};
};
