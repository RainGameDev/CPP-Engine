#pragma once
#include "assets/asset_loader.h"
#include "assets/asset_manager.h"
#include <nlohmann/json.hpp>
#include <string>

template <typename AssetType> struct Handle {
  // serialized
  std::string assetName;
  // cached lookup, not serialized
  AssetType *value = nullptr;

  AssetType *get() const { return value; }
  AssetType *operator->() const { return value; }
  explicit operator bool() const { return value != nullptr; }
};

template <typename AssetType>
bool resolve(AssetManager &am, Handle<AssetType> &h) {
  auto *loader = am.getLoader<AssetType>();
  h.value = loader ? loader->tryGetAsset(h.assetName) : nullptr;
  return h.value != nullptr;
}

template <typename T> void to_json(nlohmann::json &j, const Handle<T> &h) {
  j = h.assetName;
}
template <typename T> void from_json(const nlohmann::json &j, Handle<T> &h) {
  h = Handle<T>{};
  j.get_to(h.assetName);
}
