#include "ui/editor.h"
#include "assets/asset_manager.h"
#include "assets/material_loader.h"
#include "assets/model_loader.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "ecs/components.h"
#include "ecs/entity.h"
#include "ecs/input_manager.h"
#include "ecs/query.h"
#include "ecs/serialize.h"
#include "ecs/system_registry.h"
#include "ecs/world.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_render_context.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <print>
#include <string>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

void topbar(World &world) {
  ImGuiViewport *mainViewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(mainViewport->WorkPos);
  ImGui::SetNextWindowSize(mainViewport->WorkSize);
  ImGui::SetNextWindowViewport(mainViewport->ID);

  ImGuiWindowFlags hostFlags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("DockSpace Host", nullptr, hostFlags);
  ImGui::PopStyleVar(3);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) { /* ... */
      }
      if (ImGui::MenuItem("Open...")) { /* ... */
      }
      if (ImGui::MenuItem("Save")) {
        json saved = save_world(world);
        std::filesystem::create_directories("assets");
        std::string path =
            "assets/" + world.currentScene.sceneName + ".scene.json";
        std::ofstream file(path);
        file << saved.dump(2);
        std::print("Saved scene to {}\n", path);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) { /* ... */
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) { /* ... */
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) { /* ... */
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspaceId, ImVec2(0, 0));

  static bool dockspaceInitialized = false;
  if (!dockspaceInitialized) {
    dockspaceInitialized = true;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, mainViewport->Size);

    ImGuiID right;
    ImGuiID bottom;
    ImGuiID central;
    ImGuiID left;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, &right,
                                &central);
    ImGui::DockBuilderSplitNode(central, ImGuiDir_Down, 0.25f, &bottom,
                                &central);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.4f, &right, &left);

    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Hierarchy", right);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Preview", left);
    ImGui::DockBuilderDockWindow("Viewport", central);

    ImGui::DockBuilderFinish(dockspaceId);
  }

  ImGui::End();
}
UPDATE_SYSTEM(topbar);

void hierarchy(World &world) {
  ImGui::Begin("Hierarchy");

  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (ImGui::Button("New Entity")) {
    world.create_entity();
  }

  ImGui::Separator();

  Query<NameComponent> nameQuery(world);
  nameQuery.for_each([&editorState](EntityId id, NameComponent &nameComp) {
    if (ImGui::Button(
            (nameComp.name + " (" + std::to_string(id) + ")").c_str())) {
      editorState.selectedID = id;
    }
  });

  ImGui::End();
}
UPDATE_SYSTEM(hierarchy);

void inspector(World &world) {
  ImGui::Begin("Inspector");

  EditorStatus &editorState = *world.get_resource<EditorStatus>();

  if (editorState.selectedID) {
    std::string name =
        world.get_component<NameComponent>(editorState.selectedID)->name;
    ImGui::Text("%s", name.c_str());

    world.inspect_entity(editorState.selectedID);

    ImGui::Separator();
  }

  ImGui::End();
}
UPDATE_SYSTEM(inspector);

void assets(World &world) {
  EditorStatus &editorState = *world.get_resource<EditorStatus>();
  AssetManager &assetManager = *world.get_resource<AssetManager>();
  std::vector<IAssetLoader::AssetEntry> allAssets;
  for (auto &loader : assetManager.loaders) {
    auto entries = loader->getAllAssetEntries();
    allAssets.insert(allAssets.end(), entries.begin(), entries.end());
  }
  ImGui::Begin("Assets");
  ImGui::BeginChild("left pane", ImVec2(150, 0), true);
  ImGui::Text("Categories");
  ImGui::Separator();
  if (ImGui::Button("All")) {
    editorState.selectedAssetLoader = nullptr;
  }
  if (ImGui::Button("Materials")) {
    editorState.selectedAssetLoader = assetManager.getLoader<MaterialAsset>();
  }
  if (ImGui::Button("Models")) {
    editorState.selectedAssetLoader = assetManager.getLoader<MeshAsset>();
  }
  if (ImGui::Button("Shaders")) {
    editorState.selectedAssetLoader = assetManager.getLoader<ShaderAsset>();
  }
  if (ImGui::Button("Textures")) {
    editorState.selectedAssetLoader = assetManager.getLoader<TextureAsset>();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("right pane", ImVec2(0, 0), true);
  static char nameBuf[256];
  strncpy(nameBuf, editorState.assetSearch.c_str(), sizeof(nameBuf));
  ImGui::Text("Filter:");
  ImGui::SameLine();
  if (ImGui::InputText("##filter", nameBuf, sizeof(nameBuf))) {
    editorState.assetSearch = nameBuf;
  }
  ImGui::Separator();
  for (auto &entry : allAssets) {
    if (editorState.selectedAssetLoader != nullptr &&
        entry.loaderFrom != editorState.selectedAssetLoader)
      continue;

    float windowVisibleX2 =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    bool isSelected = (editorState.selectedAsset == entry.name);
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;
    int textLines = std::max(1, (int)std::ceil(textWidth / 128.0f));
    float textHeight = textLines * ImGui::GetTextLineHeightWithSpacing();
    ImVec2 groupSize(128, 128 + textHeight);

    if (editorState.assetSearch != "" &&
        !entry.name.contains(editorState.assetSearch)) {
      continue;
    }

    if (auto *texLoader =
            dynamic_cast<const TextureLoader *>(entry.loaderFrom)) {
      const TextureAsset &asset = texLoader->getAsset(entry.name);
      if (!asset.imguiDS)
        continue;

      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();
      ImGui::Image((ImTextureID)asset.imguiDS, ImVec2(128, 128));
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }

    else if (auto *modelLoader =
                 dynamic_cast<const ModelLoader *>(entry.loaderFrom)) {
      const MeshAsset &asset = modelLoader->getAsset(entry.name);
      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();

      ImGui::Image((ImTextureID)asset.previewTexture,
                   ImVec2(MeshAsset::kPreviewSize, MeshAsset::kPreviewSize));

      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }

    else if (dynamic_cast<const ShaderLoader *>(entry.loaderFrom)) {
      ImGui::PushID(entry.name.c_str());

      if (ImGui::Selectable("##sel", isSelected, ImGuiSelectableFlags_None,
                            groupSize)) {
        editorState.selectedAsset = entry.name;
      }

      ImGui::SetCursorScreenPos(startPos);
      ImGui::BeginGroup();
      ImVec2 iconMin = ImGui::GetCursorScreenPos();
      ImVec2 iconMax(iconMin.x + 128, iconMin.y + 128);
      ImDrawList *drawList = ImGui::GetWindowDrawList();
      drawList->AddRectFilled(iconMin, iconMax, IM_COL32(80, 60, 140, 255),
                              4.0f);
      ImVec2 textSize = ImGui::CalcTextSize("S");
      ImVec2 textPos(iconMin.x + (128 - textSize.x) * 0.5f,
                     iconMin.y + (128 - textSize.y) * 0.5f);
      drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "S");
      ImGui::Dummy(ImVec2(128, 128));
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 128);
      ImGui::TextWrapped("%s", entry.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();

      ImGui::PopID();
    }
    float lastItemX2 = ImGui::GetItemRectMax().x;
    float nextItemX2 = lastItemX2 + ImGui::GetStyle().ItemSpacing.x + 128.0f;
    if (nextItemX2 < windowVisibleX2)
      ImGui::SameLine();
  }

  ImGui::EndChild();
  ImGui::End();
}
UPDATE_SYSTEM(assets);

void viewport(World &world) {
  auto *renderer = world.get_resource<VulkanRenderingContext *>();
  auto *status = world.get_resource<EditorStatus>();
  auto *input = world.get_resource<InputManager>();
  if (!renderer || !status || !input)
    return;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Viewport", nullptr,
               ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImVec2 size = ImGui::GetContentRegionAvail();
  if (size.x > 0 && size.y > 0) {
    (*renderer)->pendingViewportExtent = {static_cast<uint32_t>(size.x),
                                          static_cast<uint32_t>(size.y)};
    ImGui::Image(
        reinterpret_cast<ImTextureID>((*renderer)->viewportDescriptorSet),
        size);
  }

  if (!input->isKeybindActive("camera_hold"))
    status->isViewportHovered = ImGui::IsWindowHovered();

  ImGui::End();
  ImGui::PopStyleVar();
}
UPDATE_SYSTEM(viewport);

// ============================================================================
// Model Asset Preview Panel
// ============================================================================

struct PreviewRenderContext {
  vk::raii::Device *device = nullptr;
  vk::raii::PhysicalDevice *physicalDevice = nullptr;
  vk::raii::CommandPool *commandPool = nullptr;
  vk::raii::Queue *queue = nullptr;

  // Preview render target
  vk::raii::Image colorImage{nullptr};
  vk::raii::DeviceMemory colorMemory{nullptr};
  vk::raii::ImageView colorView{nullptr};
  vk::raii::Image depthImage{nullptr};
  vk::raii::DeviceMemory depthMemory{nullptr};
  vk::raii::ImageView depthView{nullptr};
  vk::raii::Sampler sampler{nullptr};
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  vk::Extent2D extent{512, 512};

  // Camera for preview
  float yaw = 45.0f;
  float pitch = -20.0f;
  float distance = 3.0f;
  glm::vec3 target{0.0f};
  bool isDragging = false;
  ImVec2 lastMousePos{0, 0};
};

static void createPreviewRenderTarget(PreviewRenderContext &ctx, uint32_t width,
                                      uint32_t height, vk::Format colorFormat,
                                      vk::Format depthFormat) {
  ctx.extent = {width, height};

  vk::ImageCreateInfo colorInfo{.imageType = vk::ImageType::e2D,
                                .format = colorFormat,
                                .extent = {width, height, 1},
                                .mipLevels = 1,
                                .arrayLayers = 1,
                                .samples = vk::SampleCountFlagBits::e1,
                                .tiling = vk::ImageTiling::eOptimal,
                                .usage =
                                    vk::ImageUsageFlagBits::eColorAttachment |
                                    vk::ImageUsageFlagBits::eSampled,
                                .initialLayout = vk::ImageLayout::eUndefined};

  auto colorImg = ctx.device->createImage(colorInfo);
  auto memReqs = colorImg.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReqs.size, .memoryTypeIndex = [&]() {
        vk::PhysicalDeviceMemoryProperties memProps =
            ctx.physicalDevice->getMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
          if ((memReqs.memoryTypeBits & (1 << i)) &&
              (memProps.memoryTypes[i].propertyFlags &
               vk::MemoryPropertyFlagBits::eDeviceLocal)) {
            return i;
          }
        }
        return 0u;
      }()};
  ctx.colorMemory = ctx.device->allocateMemory(allocInfo);
  ctx.colorImage = ctx.device->createImage(colorInfo);
  ctx.colorImage.bindMemory(*ctx.colorMemory, 0);

  vk::ImageViewCreateInfo viewInfo{
      .image = *ctx.colorImage,
      .viewType = vk::ImageViewType::e2D,
      .format = colorFormat,
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  ctx.colorView = ctx.device->createImageView(viewInfo);

  vk::ImageCreateInfo depthInfo{
      .imageType = vk::ImageType::e2D,
      .format = depthFormat,
      .extent = {width, height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .initialLayout = vk::ImageLayout::eUndefined};
  auto depthImg = ctx.device->createImage(depthInfo);
  memReqs = depthImg.getMemoryRequirements();
  allocInfo.allocationSize = memReqs.size;
  ctx.depthMemory = ctx.device->allocateMemory(allocInfo);
  ctx.depthImage = ctx.device->createImage(depthInfo);
  ctx.depthImage.bindMemory(*ctx.depthMemory, 0);

  viewInfo.image = *ctx.depthImage;
  viewInfo.format = depthFormat;
  viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
  ctx.depthView = ctx.device->createImageView(viewInfo);

  vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eClampToEdge,
      .addressModeV = vk::SamplerAddressMode::eClampToEdge,
      .addressModeW = vk::SamplerAddressMode::eClampToEdge,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 1.0f,
      .minLod = 0.0f,
      .maxLod = 1.0f};
  ctx.sampler = ctx.device->createSampler(samplerInfo);
}

static void initPreviewContext(PreviewRenderContext &ctx,
                               vk::raii::Device &device,
                               vk::raii::PhysicalDevice &physicalDevice,
                               vk::raii::CommandPool &commandPool,
                               vk::raii::Queue &queue, uint32_t width,
                               uint32_t height, vk::Format colorFormat,
                               vk::Format depthFormat) {
  ctx.device = &device;
  ctx.physicalDevice = &physicalDevice;
  ctx.commandPool = &commandPool;
  ctx.queue = &queue;

  createPreviewRenderTarget(ctx, width, height, colorFormat, depthFormat);

  ctx.descriptorSet = ImGui_ImplVulkan_AddTexture(
      *ctx.sampler, *ctx.colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void cleanupPreviewContext(PreviewRenderContext &ctx) {
  if (ctx.descriptorSet != VK_NULL_HANDLE) {
    ctx.device->waitIdle();
    ImGui_ImplVulkan_RemoveTexture(ctx.descriptorSet);
    ctx.descriptorSet = VK_NULL_HANDLE;
  }
}

static void renderPreviewMesh(PreviewRenderContext &ctx, const MeshAsset &asset,
                              const MaterialAsset *material,
                              VulkanRenderingContext &renderer) {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = *ctx.commandPool,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  vk::raii::CommandBuffer cmdBuf =
      std::move(ctx.device->allocateCommandBuffers(allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  cmdBuf.begin(beginInfo);

  vk::ImageMemoryBarrier2 colorBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
      .srcAccessMask = {},
      .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
      .oldLayout = vk::ImageLayout::eUndefined,
      .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = *ctx.colorImage,
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  vk::ImageMemoryBarrier2 depthBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
      .srcAccessMask = {},
      .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
      .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      .oldLayout = vk::ImageLayout::eUndefined,
      .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = *ctx.depthImage,
      .subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}};
  std::array<vk::ImageMemoryBarrier2, 2> barriers = {colorBarrier,
                                                     depthBarrier};
  vk::DependencyInfo depInfo{.imageMemoryBarrierCount = 2,
                             .pImageMemoryBarriers = barriers.data()};
  cmdBuf.pipelineBarrier2(depInfo);

  vk::ClearValue clearColor{vk::ClearColorValue(0.15f, 0.15f, 0.18f, 1.0f)};
  vk::ClearValue clearDepth{vk::ClearDepthStencilValue(1.0f, 0)};
  std::array<vk::ClearValue, 2> clearValues{clearColor, clearDepth};

  vk::RenderingAttachmentInfo colorAttachment{
      .imageView = *ctx.colorView,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};
  vk::RenderingAttachmentInfo depthAttachment{
      .imageView = *ctx.depthView,
      .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth};

  vk::RenderingInfo renderInfo{.renderArea = {{0, 0}, ctx.extent},
                               .layerCount = 1,
                               .colorAttachmentCount = 1,
                               .pColorAttachments = &colorAttachment,
                               .pDepthAttachment = &depthAttachment};

  cmdBuf.beginRendering(renderInfo);

  cmdBuf.setViewport(
      0, vk::Viewport{0.0f, 0.0f, static_cast<float>(ctx.extent.width),
                      static_cast<float>(ctx.extent.height), 0.0f, 1.0f});
  cmdBuf.setScissor(0, vk::Rect2D{{0, 0}, ctx.extent});

  // Use the main renderer's pipeline layout and material pipeline
  ShaderKey key;
  if (material) {
    key = {material->vertexShader, material->fragmentShader};
  } else {
    key = {"sdr_default_model.vert", "sdr_default_model.frag"};
  }
  vk::Pipeline pipeline = renderer.getOrCreatePipeline(key);

  cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

  // Compute camera matrices
  float radYaw = glm::radians(ctx.yaw);
  float radPitch = glm::radians(ctx.pitch);
  glm::vec3 camPos =
      ctx.target + glm::vec3{ctx.distance * cosf(radPitch) * sinf(radYaw),
                             ctx.distance * sinf(radPitch),
                             ctx.distance * cosf(radPitch) * cosf(radYaw)};
  glm::mat4 view = glm::lookAt(camPos, ctx.target, glm::vec3(0, 1, 0));
  glm::mat4 proj = glm::perspective(
      glm::radians(45.0f),
      static_cast<float>(ctx.extent.width) / ctx.extent.height, 0.1f, 100.0f);
  proj[1][1] *= -1; // Vulkan clip space Y flip
  glm::mat4 model = glm::mat4(1.0f);
  glm::mat4 mvp = proj * view * model;

  // Material push constants
  MaterialPushConstants pc{
      .baseColorFactor = material ? material->baseColorFactor : glm::vec4(1.0f),
      .metallicFactor = material ? material->metallicFactor : 1.0f,
      .roughnessFactor = material ? material->roughnessFactor : 1.0f,
      .parallaxStrength = material ? material->parallaxStrength : 0.0f};
  cmdBuf.pushConstants(*renderer.pipelineLayout,
                       vk::ShaderStageFlagBits::eVertex |
                           vk::ShaderStageFlagBits::eFragment,
                       0, sizeof(pc), &pc);

  // Bind descriptor sets: set 0 (per-frame) and set 1 (material)
  if (material) {
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *renderer.pipelineLayout, 0,
        {renderer.descriptorSets[renderer.currentFrame],
         *material->descriptorSet},
        {0}); // dynamic offset 0 for transform (not used in preview)
  } else {
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *renderer.pipelineLayout, 0,
        {renderer.descriptorSets[renderer.currentFrame]}, {0});
  }

  cmdBuf.bindVertexBuffers(0, {*asset.mesh.vertexBuffer}, {0});
  cmdBuf.bindIndexBuffer(*asset.mesh.indexBuffer, 0, vk::IndexType::eUint32);
  cmdBuf.drawIndexed(asset.mesh.indexCount, 1, 0, 0, 0);

  cmdBuf.endRendering();

  // Transition color image to shader read
  vk::ImageMemoryBarrier2 toShaderRead{
      .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
      .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = *ctx.colorImage,
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  vk::DependencyInfo depInfo2{.imageMemoryBarrierCount = 1,
                              .pImageMemoryBarriers = &toShaderRead};
  cmdBuf.pipelineBarrier2(depInfo2);

  cmdBuf.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*cmdBuf};
  ctx.queue->submit(submitInfo, nullptr);
  ctx.queue->waitIdle();
}

static void previewPanel(World &world) {
  EditorStatus &editorState = *world.get_resource<EditorStatus>();
  AssetManager &assetManager = *world.get_resource<AssetManager>();
  auto *rendererPtr = world.get_resource<VulkanRenderingContext *>();
  if (!rendererPtr || !*rendererPtr)
    return;

  VulkanRenderingContext &renderer = **rendererPtr;

  static PreviewRenderContext previewCtx;
  static bool previewInitialized = false;

  ImGui::Begin("Preview");

  if (editorState.selectedAsset.empty() ||
      editorState.selectedAssetLoader != assetManager.getLoader<MeshAsset>()) {
    ImGui::TextDisabled("No model asset selected");
    ImGui::TextDisabled("Select a model in the Assets panel to preview");
    ImGui::End();
    return;
  }

  auto *modelLoader =
      dynamic_cast<ModelLoader *>(editorState.selectedAssetLoader);
  if (!modelLoader) {
    ImGui::TextDisabled("Invalid model loader");
    ImGui::End();
    return;
  }

  const MeshAsset &asset = modelLoader->getAsset(editorState.selectedAsset);

  ImVec2 avail = ImGui::GetContentRegionAvail();
  uint32_t width = static_cast<uint32_t>(std::max(256.0f, avail.x));
  uint32_t height = static_cast<uint32_t>(std::max(256.0f, avail.y));

  if (!previewInitialized || previewCtx.extent.width != width ||
      previewCtx.extent.height != height) {
    if (previewInitialized) {
      cleanupPreviewContext(previewCtx);
    }
    initPreviewContext(previewCtx, renderer.device, renderer.physicalDevice,
                       renderer.commandPool, renderer.graphicsQueue, width,
                       height, renderer.swapChainSurfaceFormat.format,
                       renderer.depthFormat);
    previewInitialized = true;
  }

  MaterialAsset *material = asset.mesh.material ? asset.mesh.material : nullptr;

  // Mouse interaction for orbit camera
  bool hovered = ImGui::IsWindowHovered();
  if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImVec2 mousePos = ImGui::GetMousePos();
    if (!previewCtx.isDragging) {
      previewCtx.isDragging = true;
      previewCtx.lastMousePos = mousePos;
    } else {
      float dx = mousePos.x - previewCtx.lastMousePos.x;
      float dy = mousePos.y - previewCtx.lastMousePos.y;
      previewCtx.yaw += dx * 0.5f;
      previewCtx.pitch =
          glm::clamp(previewCtx.pitch - dy * 0.5f, -89.0f, 89.0f);
      previewCtx.lastMousePos = mousePos;
    }
  } else {
    previewCtx.isDragging = false;
  }

  if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
    previewCtx.distance = glm::clamp(
        previewCtx.distance - ImGui::GetIO().MouseWheel * 0.5f, 0.5f, 50.0f);
  }

  // Render preview using main renderer's pipeline
  renderPreviewMesh(previewCtx, asset, material, renderer);

  // Display preview
  ImGui::Image(reinterpret_cast<ImTextureID>(previewCtx.descriptorSet),
               ImVec2(width, height));

  // Info
  ImGui::Separator();
  ImGui::Text("Model: %s", editorState.selectedAsset.c_str());
  ImGui::Text("Vertices: %u", asset.mesh.vertexCount);
  ImGui::Text("Indices: %u", asset.mesh.indexCount);
  ImGui::Text("Material: %s", material ? "assigned" : "none (default)");
  ImGui::Separator();
  ImGui::TextDisabled("LMB: Orbit | Scroll: Zoom");

  ImGui::End();
}
UPDATE_SYSTEM(previewPanel);
