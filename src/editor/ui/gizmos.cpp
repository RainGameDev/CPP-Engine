#include <cstdio>
#define GLM_ENABLE_EXPERIMENTAL

#include "imgui.h"
#include "imgui_internal.h"

#include "ImGuizmo.h"
#include "ecs/components.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "rendering/camera.h"
#include "ui/editor.h"
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>

void gizmos(World &world) {
  auto *status = world.get_resource<EditorStatus>();
  auto *editorCam = world.get_resource<EditorCamera>();
  if (!status || !editorCam)
    return;

  if (ImGui::IsKeyPressed(ImGuiKey_W))
    status->currentOp = ImGuizmo::TRANSLATE;
  if (ImGui::IsKeyPressed(ImGuiKey_E))
    status->currentOp = ImGuizmo::ROTATE;
  if (ImGui::IsKeyPressed(ImGuiKey_R))
    status->currentOp = ImGuizmo::SCALE;

  ImGuizmo::BeginFrame();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Viewport", nullptr,
               ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
  ImGuizmo::SetDrawlist();

  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
  ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
  ImVec2 gizmoPos(ImGui::GetWindowPos().x + contentMin.x,
                  ImGui::GetWindowPos().y + contentMin.y);
  ImVec2 gizmoSize(contentMax.x - contentMin.x, contentMax.y - contentMin.y);

  if (status->selectedID != NONE && gizmoSize.x > 0 && gizmoSize.y > 0) {
    auto *tc = world.get_component<TransformComponent>(status->selectedID);
    if (tc) {
      float aspect = gizmoSize.x / gizmoSize.y;
      glm::mat4 model = tc->getMatrix();
      glm::mat4 view = editorCam->cam.getViewMatrix(editorCam->transform);
      glm::mat4 proj = editorCam->cam.getProjectionMatrix(aspect);

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetRect(gizmoPos.x, gizmoPos.y, gizmoSize.x, gizmoSize.y);

      ImGuizmo::MODE currentMode = (status->currentOp == ImGuizmo::SCALE)
                                       ? ImGuizmo::LOCAL
                                       : ImGuizmo::WORLD;

      ImGuizmo::Manipulate(&view[0][0], &proj[0][0], status->currentOp,
                           currentMode, &model[0][0]);

      if (ImGuizmo::IsUsing()) {
        glm::vec3 pos, scl, skew;
        glm::vec4 persp;
        glm::quat rot;
        glm::decompose(model, scl, rot, pos, skew, persp);
        tc->position = pos;
        tc->rotation = rot;
        tc->scale = scl;
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(gizmos);
