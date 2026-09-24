#include "assets/asset_manager.h"
#include "assets/asset_payload.h"
#include "assets/model_loader.h"
#include "ecs/components.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "rendering/camera.h"
#include "rendering/vulkan_render_context.h"
#include "ui/editor.h"
#include "ui/shared.h"
#include <cstdint>
#include <glm/glm.hpp>

struct Tool {
  const char *label;
  const char *key;
  ImGuizmo::OPERATION op;
};
static const Tool tools[] = {
    {"Move", "W", ImGuizmo::TRANSLATE},
    {"Rotate", "E", ImGuizmo::ROTATE},
    {"Scale", "R", ImGuizmo::SCALE},
};

static glm::vec3 viewportDropPosition(World &world, ImVec2 rectPos,
                                      ImVec2 rectSize) {
  auto *editorCam = world.get_resource<EditorCamera>();
  if (!editorCam || rectSize.x <= 0.0f || rectSize.y <= 0.0f)
    return glm::vec3(0.0f);

  float aspect = rectSize.x / rectSize.y;
  glm::mat4 view = editorCam->cam.getViewMatrix(editorCam->transform);
  glm::mat4 proj = editorCam->cam.getProjectionMatrix(aspect);
  glm::mat4 invViewProj = glm::inverse(proj * view);

  ImVec2 mouse = ImGui::GetIO().MousePos;
  float ndcX = (mouse.x - rectPos.x) / rectSize.x * 2.0f - 1.0f;
  float ndcY = 1.0f - (mouse.y - rectPos.y) / rectSize.y * 2.0f;

  glm::vec4 farClip = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
  glm::vec3 far = glm::vec3(farClip) / farClip.w;
  glm::vec3 origin = editorCam->cam.getPosition(editorCam->transform);
  glm::vec3 dir = glm::normalize(far - origin);

  if (glm::abs(dir.y) > 1e-4f) {
    float t = (0.0f - origin.y) / dir.y;
    if (t > 0.0f)
      return origin + dir * t;
  }
  return origin + dir * 5.0f;
}

static void spawnEntityFromMeshDrop(World &world,
                                    const AssetDragPayload &payload,
                                    glm::vec3 position) {
  auto *assetManager = world.get_resource<AssetManager>();
  auto *modelLoader =
      assetManager ? assetManager->getLoader<MeshAsset>() : nullptr;
  if (!assetManager || !modelLoader || !modelLoader->tryGetAsset(payload.name))
    return;

  recordUndo(world);
  EntityId id = world.create_entity();
  world.add_component(id, NameComponent{.name = payload.name});
  MeshComponent mc;
  mc.mesh.assetName = payload.name;
  resolve(*assetManager, mc.mesh);
  resolve(*assetManager, mc.overrideMaterial);
  world.add_component(id, std::move(mc));
  world.add_component(id, TransformComponent{.position = position});

  if (auto *status = world.get_resource<EditorStatus>())
    status->selectedID = id;
}

void viewport(World &world) {
  auto *renderer = world.get_resource<VulkanRenderingContext *>();
  auto *status = world.get_resource<EditorStatus>();
  if (!renderer || !status)
    return;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  bool viewportOpen = ImGui::Begin("Viewport", nullptr,
                                   ImGuiWindowFlags_NoBackground |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (!viewportOpen) {
    // Tabbed behind another window in the same dock nodem nothing to draw.
    status->isViewportHovered = false;
    ImGui::End();
    ImGui::PopStyleVar();
    return;
  }
  ImVec2 size = ImGui::GetContentRegionAvail();
  ImVec2 imageMin{0, 0};
  ImVec2 imageSize{0, 0};
  if (size.x > 0 && size.y > 0) {
    (*renderer)->pendingViewportExtent = {static_cast<uint32_t>(size.x),
                                          static_cast<uint32_t>(size.y)};
    imageMin = ImGui::GetCursorScreenPos();
    imageSize = size;
    ImGui::Image(
        reinterpret_cast<ImTextureID>((*renderer)->viewportDescriptorSet),
        size);

    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *payload =
              ImGui::AcceptDragDropPayload(AssetDragPayload::kPayloadType)) {
        const auto &drag =
            *static_cast<const AssetDragPayload *>(payload->Data);
        if (drag.type == AssetDragPayload::Mesh) {
          glm::vec3 dropPos = viewportDropPosition(world, imageMin, size);
          spawnEntityFromMeshDrop(world, drag, dropPos);
        }
      }
      ImGui::EndDragDropTarget();
    }

    ImGuiStyle &style = ImGui::GetStyle();
    const float btnH = ImGui::GetFrameHeight();
    const int n = IM_ARRAYSIZE(tools);

    // Uniform box width from the widest tool label.
    float labelW = 0.0f;
    for (auto &t : tools) {
      float w = ImGui::CalcTextSize(t.label).x;
      if (w > labelW)
        labelW = w;
    }
    const float btnW = labelW + style.FramePadding.x * 2.0f;

    const float barW = btnW + style.FramePadding.x * 2.0f;
    const float barH = n * btnH + (n - 1) * style.ItemSpacing.y + 10.0 +
                       style.FramePadding.y * 2.0f;

    // Left edge of the viewport, near the top.
    ImVec2 barMin = {imageMin.x + 6.0f, imageMin.y + 9.0f};
    ImVec2 barMax = {barMin.x + barW, barMin.y + barH};

    status->toolbarRectMin = barMin;
    status->toolbarRectMax = barMax;

    ImGui::SetCursorScreenPos(barMin);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::BeginChild("##viewportToolbar", {barW, barH},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_FrameStyle,
                      ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    for (int i = 0; i < n; ++i) {
      if (i)
        ImGui::Spacing();
      bool active = !status->handTool && status->currentOp == tools[i].op;
      if (active)
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
      if (ImGui::Button(tools[i].label, {btnW, btnH})) {
        status->handTool = false;
        status->currentOp = tools[i].op;
      }
      if (active)
        ImGui::PopStyleColor();
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s (%s)", tools[i].label, tools[i].key);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
  }

  if (imageSize.x > 0 && imageSize.y > 0)
    drawViewportGizmos(world, imageMin, imageSize);

  status->isViewportHovered =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);
