#pragma once
#include "imgui.h"

#include "ImGuizmo.h"
#include "assets/asset_loader.h"
#include "assets/asset_payload.h"
#include "ecs/entity.h"
#include <optional>
#include <string>

class World;
void topbar(World &world);

struct EditorStatus {
  EntityId selectedID = NONE;
  IAssetLoader *selectedAssetLoader = nullptr;
  bool isViewportHovered = false;
  std::string assetSearch;
  std::string selectedAsset;
  std::string hoveredAsset;

  ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE;
};
