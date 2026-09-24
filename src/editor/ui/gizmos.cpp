#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#define GLM_ENABLE_EXPERIMENTAL

#include "imgui.h"
#include "imgui_internal.h"

#include "ImGuizmo.h"
#define IMVIEWGUIZMO_IMPLEMENTATION
#include "ImViewGuizmo.h"
#include "ecs/components.h"
#include "ecs/light.h"
#include "ecs/query.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "rendering/camera.h"
#include "ui/editor.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

void drawLightGizmos(World &world, EditorStatus *status, const glm::mat4 &view,
                     const glm::mat4 &proj, ImVec2 rectPos, ImVec2 rectSize);
static void drawViewGizmo(World &world, const glm::vec3 &pivot, ImVec2 rectPos,
                          ImVec2 rectSize);

void drawViewportGizmos(World &world, ImVec2 gizmoPos, ImVec2 gizmoSize) {
  auto *status = world.get_resource<EditorStatus>();
  auto *editorCam = world.get_resource<EditorCamera>();
  if (!status || !editorCam)
    return;
  if (gizmoSize.x <= 0 || gizmoSize.y <= 0)
    return;

  if (ImGui::IsKeyPressed(ImGuiKey_W))
    status->currentOp = ImGuizmo::TRANSLATE;
  if (ImGui::IsKeyPressed(ImGuiKey_E))
    status->currentOp = ImGuizmo::ROTATE;
  if (ImGui::IsKeyPressed(ImGuiKey_R))
    status->currentOp = ImGuizmo::SCALE;

  ImGuizmo::BeginFrame();
  ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
  // Window drawlist so gizmos live inside the viewport.
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

  {
    float aspect = gizmoSize.x / gizmoSize.y;
    glm::mat4 view = editorCam->cam.getViewMatrix(editorCam->transform);
    glm::mat4 proj = editorCam->cam.getProjectionMatrix(aspect);

    drawLightGizmos(world, status, view, proj, gizmoPos, gizmoSize);
    glm::vec3 pivot(0.0f);
    if (status->selectedID != NONE) {
      auto *tc = world.get_component<TransformComponent>(status->selectedID);
      if (tc)
        pivot = tc->position;
    }
    drawViewGizmo(world, pivot, gizmoPos, gizmoSize);

    if (status->selectedID != NONE) {
      auto *tc = world.get_component<TransformComponent>(status->selectedID);
      if (tc) {
        glm::mat4 model = tc->getMatrix();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(gizmoPos.x, gizmoPos.y, gizmoSize.x, gizmoSize.y);

        ImGuizmo::MODE currentMode = (status->currentOp == ImGuizmo::SCALE)
                                         ? ImGuizmo::LOCAL
                                         : ImGuizmo::WORLD;

        bool allowInteract =
            ImGuizmo::IsUsing() ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        ImGuizmo::Enable(allowInteract);
        ImGuizmo::Manipulate(&view[0][0], &proj[0][0], status->currentOp,
                             currentMode, &model[0][0]);
        ImGuizmo::Enable(true);

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
  }
}

static bool worldToScreen(const glm::vec3 &world, const glm::mat4 &viewProj,
                          ImVec2 rectPos, ImVec2 rectSize, ImVec2 &outScreen) {
  glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
  // at/behind the camera plane
  if (clip.w <= glm::epsilon<float>())
    return false;

  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  outScreen.x = rectPos.x + (ndc.x * 0.5f + 0.5f) * rectSize.x;
  outScreen.y = rectPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * rectSize.y;
  return true;
}

/// Projects a screen point from clip space.
static ImVec2 projectClip(const glm::vec4 &clip, ImVec2 rectPos,
                          ImVec2 rectSize) {
  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  return {rectPos.x + (ndc.x * 0.5f + 0.5f) * rectSize.x,
          rectPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * rectSize.y};
}

/// Draws segment, clipped to the camera's near plane.
static void drawClippedSegment(ImDrawList *drawList, const glm::vec3 &a,
                               const glm::vec3 &b, const glm::mat4 &viewProj,
                               ImVec2 rectPos, ImVec2 rectSize, ImU32 color,
                               float thickness) {
  glm::vec4 ca = viewProj * glm::vec4(a, 1.0f);
  glm::vec4 cb = viewProj * glm::vec4(b, 1.0f);

  const float e = 1e-5f;
  // fully behind the camera
  if (ca.w <= e && cb.w <= e)
    return;
  // A behind clip to near plane, project it just past
  if (ca.w < e)
    ca = glm::vec4(glm::vec3(ca + (cb - ca) * (ca.w / (ca.w - cb.w))), e);
  else if (cb.w < e)
    cb = glm::vec4(glm::vec3(cb + (ca - cb) * (cb.w / (cb.w - ca.w))), e);

  drawList->AddLine(projectClip(ca, rectPos, rectSize),
                    projectClip(cb, rectPos, rectSize), color, thickness);
}

void drawLightGizmos(World &world, EditorStatus *status, const glm::mat4 &view,
                     const glm::mat4 &proj, ImVec2 rectPos, ImVec2 rectSize) {
  glm::mat4 viewProj = proj * view;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->PushClipRect(
      rectPos, {rectPos.x + rectSize.x, rectPos.y + rectSize.y}, true);

  Query<TransformComponent, LightComponent> lightQuery(world);
  lightQuery.for_each([&](EntityId id, TransformComponent &tc,
                          LightComponent &light) {
    bool selected = (status->selectedID == id);
    ImU32 color =
        IM_COL32((int)(light.color.r * 255.0f), (int)(light.color.g * 255.0f),
                 (int)(light.color.b * 255.0f), 255);

    ImVec2 screenPos;
    if (worldToScreen(tc.position, viewProj, rectPos, rectSize, screenPos)) {
      // Icon filled circle for the light origin
      drawList->AddCircleFilled(screenPos, 6.0f, color);
      drawList->AddCircle(screenPos, 8.0f, IM_COL32(0, 0, 0, 180), 12, 1.5f);

      // check mouse distance to icon
      ImVec2 mouse = ImGui::GetIO().MousePos;
      float dist =
          ImLengthSqr(ImVec2(mouse.x - screenPos.x, mouse.y - screenPos.y));
      if (dist < 100.0f && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
          ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        status->selectedID = id;
      }
    }

    // Directional lights always show their ray fan; point/spot geometry is
    // only drawn for the selected light.
    if (std::holds_alternative<Directional>(light.lightType)) {
      glm::vec3 dir = glm::normalize(tc.rotation * glm::vec3(0, 0, -1));
      glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
      glm::vec3 up = glm::cross(right, dir);

      // info for the sun disk
      const float r = 0.5f;
      const int segments = 24;
      const int spikes = 10;

      glm::vec3 prev = tc.position + (right * cosf(0.0f) + up * sinf(0.0f)) * r;
      for (int i = 0; i <= segments; i++) {
        float a = (float)i / segments * glm::two_pi<float>();
        glm::vec3 p = tc.position + (right * cosf(a) + up * sinf(a)) * r;
        drawClippedSegment(drawList, prev, p, viewProj, rectPos, rectSize,
                           color, 2.0f);
        prev = p;
      }

      for (int i = 0; i < spikes; i++) {
        float a = (float)i / spikes * glm::two_pi<float>();
        glm::vec3 axis = glm::normalize(right * cosf(a) + up * sinf(a));
        drawClippedSegment(drawList, tc.position + axis * r,
                           tc.position + axis * (r * 1.12f) + dir * 1.5f,
                           viewProj, rectPos, rectSize, color, 1.5f);
      }

      drawClippedSegment(drawList, tc.position, tc.position + dir * 0.8f,
                         viewProj, rectPos, rectSize, color, 2.0f);
      return;
    }

    if (!selected)
      return;

    // Extra shape depending on light type
    if (auto *point = std::get_if<Point>(&light.lightType)) {
      // Wireframe sphere at the light's radius
      const int segments = 40;
      auto ring = [&](const glm::vec3 &center, const glm::vec3 &u,
                      const glm::vec3 &v) {
        ImVec2 prev;
        bool hasPrev = false;
        for (int i = 0; i <= segments; i++) {
          float a = (float)i / segments * glm::two_pi<float>();
          glm::vec3 p = center + u * cosf(a) + v * sinf(a);
          ImVec2 s;
          if (!worldToScreen(p, viewProj, rectPos, rectSize, s)) {
            hasPrev = false;
            continue;
          }
          if (hasPrev)
            drawList->AddLine(prev, s, color, 1.0f);
          prev = s;
          hasPrev = true;
        }
      };

      float r = point->radius;
      // Three aligned circles
      ring(tc.position, glm::vec3(r, 0, 0), glm::vec3(0, r, 0));
      ring(tc.position, glm::vec3(r, 0, 0), glm::vec3(0, 0, r));
      ring(tc.position, glm::vec3(0, r, 0), glm::vec3(0, 0, r));
      // lattude rings
      for (float phi : {-60.0f, -30.0f, 30.0f, 60.0f}) {
        float rad = glm::radians(phi);
        float cr = r * cosf(rad);
        ring(tc.position + glm::vec3(0, r * sinf(rad), 0), glm::vec3(cr, 0, 0),
             glm::vec3(0, 0, cr));
      }
    } else if (auto *spot = std::get_if<Spot>(&light.lightType)) {
      glm::vec3 dir = glm::normalize(tc.rotation * glm::vec3(0, 0, -1));
      glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
      glm::vec3 up = glm::cross(right, dir);
      float coneRadius = tanf(glm::radians(spot->angle * 0.5f)) * spot->length;

      glm::vec3 tip = tc.position + dir * spot->length;
      const int segments = 24;
      glm::vec3 prevRim;
      for (int i = 0; i <= segments; i++) {
        float a = (float)i / segments * glm::two_pi<float>();
        glm::vec3 rim = tip + (right * cosf(a) + up * sinf(a)) * coneRadius;
        // Rim segments and spokes are near plane clipped
        if (i > 0)
          drawClippedSegment(drawList, prevRim, rim, viewProj, rectPos,
                             rectSize, color, 1.0f);
        if (i % 6 == 0)
          drawClippedSegment(drawList, tc.position, rim, viewProj, rectPos,
                             rectSize, color, 1.0f);
        prevRim = rim;
      }
    }
  });
  drawList->PopClipRect();
}

static void drawViewGizmo(World &world, const glm::vec3 &pivot, ImVec2 rectPos,
                          ImVec2 rectSize) {
  auto *editorCam = world.get_resource<EditorCamera>();
  if (!editorCam)
    return;

  ImViewGuizmo::BeginFrame();
  float half = 128.0f * ImViewGuizmo::GetStyle().scale;
  const float margin = 10.0f;
  ImVec2 gizmoPos(rectPos.x + rectSize.x - half - margin,
                  rectPos.y + half + margin);

  if (ImViewGuizmo::Rotate(editorCam->transform.position,
                           editorCam->transform.rotation, pivot, gizmoPos)) {
    editorCam->cam.updateCameraVectors(editorCam->transform);
  }
}
