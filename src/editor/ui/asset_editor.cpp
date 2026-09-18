#include "assets/asset_manager.h"
#include "assets/asset_payload.h"
#include "assets/handle.h"
#include "assets/material_loader.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui.h"
#include "ui/editor.h"
#include <cstring>

void editMaterial(AssetManager &am, AssetLoader<MaterialAsset> *loader,
                  const std::string &name, MaterialAsset &mat) {
  ImGui::Text("%s", name.c_str());

  auto texField = [&](const char *label, Handle<TextureAsset> &h) {
    auto *texLoader = am.getLoader<TextureAsset>();
    TextureAsset *prev =
        texLoader ? texLoader->tryGetAsset(h.assetName) : nullptr;
    if (prev)
      resolve(am, h);
    bool changed = assetDragDropField(
        label, AssetDragPayload::Texture, am, h, 64.0f, [&](float s) {
          TextureAsset *t = h ? texLoader->tryGetAsset(h.assetName) : nullptr;
          if (t && t->imguiDS)
            ImGui::Image((ImTextureID)t->imguiDS, ImVec2(s, s));
          else {
            ImGui::Dummy(ImVec2(s, s));
          }
        });
    if (changed) {
      resolve(am, h);
      if (auto *ml = dynamic_cast<MaterialLoader *>(loader))
        ml->refreshDescriptors(mat);
    }
  };
  texField("Albedo", mat.albedo);
  texField("Normal", mat.normal);
  texField("RMAOS", mat.rmaos);
  texField("Height", mat.height);

  auto shaderField = [&](const char *label, Handle<ShaderAsset> &h) {
    char buf[128] = {};
    strncpy(buf, h.assetName.c_str(), sizeof(buf) - 1);
    if (ImGui::InputText(label, buf, sizeof(buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      h.assetName = buf;
      resolve(am, h);
    }
    assetDropTarget(AssetDragPayload::Shader, am, h);
  };
  shaderField("Vert", mat.vertexShader);
  shaderField("Frag", mat.fragmentShader);

  ImGui::ColorEdit4("BaseColor", &mat.baseColorFactor.x);
  ImGui::DragFloat("Metallic", &mat.metallicFactor, 0.01f, 0, 1);
  ImGui::DragFloat("Roughness", &mat.roughnessFactor, 0.01f, 0, 1);
  ImGui::DragFloat("Parallax", &mat.parallaxStrength, 0.001f, 0, 0.2f);

  if (ImGui::Button("Save"))
    if (auto *ml = dynamic_cast<MaterialLoader *>(loader))
      ml->saveAsset(name, mat);
}

void asset_editor(World &world) {
  ImGui::Begin("Asset Editor");
  AssetManager &am = *world.get_resource<AssetManager>();
  EditorStatus &st = *world.get_resource<EditorStatus>();
  if (st.selectedAsset.empty()) {
    ImGui::Text("Select an asset.");
    ImGui::End();
    return;
  }

  auto *matLoader = am.getLoader<MaterialAsset>();
  if (auto *mat = matLoader->tryGetAsset(st.selectedAsset)) {
    editMaterial(am, matLoader, st.selectedAsset, *mat);
  }
  ImGui::End();
}
UPDATE_SYSTEM(asset_editor)
