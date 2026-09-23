#pragma once
#include "assets/asset_loader.h"
#include "assets/asset_manager.h"
#include "assets/asset_payload.h"
#include "imgui.h"
#include <functional>
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

/// Drag-and-drop target attached to the last widget. Accepts assets of the
/// given type that exist in the AssetManager and assigns them to the handle.
/// Returns true when a valid asset was dropped.
template <typename AssetType>
bool assetDropTarget(AssetDragPayload::Type type, AssetManager &assetManager,
                     Handle<AssetType> &handle) {
  bool dropped = false;
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload(AssetDragPayload::kPayloadType)) {
      const auto &drag = *static_cast<const AssetDragPayload *>(payload->Data);
      if (drag.type == type) {
        Handle<AssetType> candidate;
        candidate.assetName = drag.name;
        if (resolve(assetManager, candidate)) {
          handle = candidate;
          dropped = true;
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  return dropped;
}

/// Renders a drag and drop field. When `contentOffsetX` > 0, the thumbnail
/// (content) starts that many pixels from the start of the row instead of
/// right after the label, allowing widgets to be column-aligned.
template <typename AssetType>
bool assetDragDropField(const char *label, AssetDragPayload::Type type,
                        AssetManager &assetManager, Handle<AssetType> &handle,
                        float previewSize = 96.0f,
                        const std::function<void(float)> &drawThumbnail = {},
                        const std::function<void()> &onRemove = {},
                        float contentOffsetX = 0.0f) {
  ImGui::PushID(("asset_field_" + std::string(label)).c_str());

  ImVec2 labelSize = ImGui::CalcTextSize(label);
  const float labelOffsetY = (previewSize - labelSize.y) * 0.5f;
  const float contentX =
      contentOffsetX > 0.0f ? contentOffsetX
                            : labelSize.x + ImGui::GetStyle().ItemSpacing.x;

  ImVec2 rowCursor = ImGui::GetCursorPos();
  ImGui::SetCursorPos(ImVec2(rowCursor.x, rowCursor.y + labelOffsetY));
  ImGui::TextUnformatted(label);
  ImGui::SetCursorPos(ImVec2(rowCursor.x + contentX, rowCursor.y));

  if (drawThumbnail)
    drawThumbnail(previewSize);

  if (ImGui::BeginPopupContextItem("ctx")) {
    if (ImGui::Button(("Remove " + std::string(label)).c_str())) {
      handle = Handle<AssetType>{};
      if (onRemove)
        onRemove();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  bool dropped = assetDropTarget(type, assetManager, handle);
  ImGui::PopID();
  return dropped;
}

template <typename T> void to_json(nlohmann::json &j, const Handle<T> &h) {
  j = h.assetName;
}
template <typename T> void from_json(const nlohmann::json &j, Handle<T> &h) {
  h = Handle<T>{};
  j.get_to(h.assetName);
}
