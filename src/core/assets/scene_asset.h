#pragma once
#include "asset_loader.h"
#include <string>

struct SceneAsset {
  std::string path;
};

class SceneLoader : public AssetLoader<SceneAsset> {
public:
  std::vector<std::string> extensions() const override { return {".scene.json"}; }

  SceneAsset loadAsset(const std::string &path) override {
    SceneAsset scene;
    scene.path = path;
    return scene;
  }
};
