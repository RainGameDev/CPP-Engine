#pragma once
#include "assets/asset_loader.h"
#include "ecs/entity.h"
#include <string>
struct EditorStatus {
  EntityId selectedID;
  IAssetLoader *selectedAssetLoader = nullptr;
  bool isViewportHovered = false;
  std::string assetSearch;
  std::string selectedAsset;
  std::string hoveredAsset;
};
