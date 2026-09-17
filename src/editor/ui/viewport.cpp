#include "assets/asset_manager.h"
#include "assets/model_loader.h"
#include "assets/asset_payload.h"
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
  ImGui::Begin("Viewport", nullptr,
               ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (size.x > 0 && size.y > 0) {
    (*renderer)->pendingViewportExtent = {static_cast<uint32_t>(size.x),
                                          static_cast<uint32_t>(size.y)};
    ImVec2 imageMin = ImGui::GetCursorScreenPos();
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
  }

  status->isViewportHovered = ImGui::IsWindowHovered();

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);
