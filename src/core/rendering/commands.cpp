#include "assets/material_loader.h"
#include "ecs/components.h"
#include "ecs/query.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_render_context.h"

#include <cstdint>
#include <iostream>
#include <vector>

void VulkanRenderingContext::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueIndex};
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

void VulkanRenderingContext::createCommandBuffer() {
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = maxConcurrentFrames};
  auto buffers = vk::raii::CommandBuffers(device, allocInfo);
  commandBuffers.clear();
  for (auto &buf : buffers) {
    commandBuffers.push_back(std::move(buf));
  }
}

void VulkanRenderingContext::recordCommandBuffer(uint32_t imageIndex,
                                                 uint32_t currentFrame) {
  auto &cmd = commandBuffers[currentFrame];
  cmd.begin({});

  // --- Pass 1: Render 3D scene ---

  transition_image_layout(cmd, *viewportColorImage, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal, {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput);

  {
    vk::ImageMemoryBarrier2 depthBarrier = {
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *viewportDepthImage,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vk::DependencyInfo depthDepInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &depthBarrier};
    cmd.pipelineBarrier2(depthDepInfo);
  }

  vk::ClearValue viewportClear = vk::ClearColorValue(0.0f, 0.1f, 0.75f, 1.0f);
  vk::ClearValue viewportDepthClear = vk::ClearDepthStencilValue(1.0f, 0);
  std::array<vk::ClearValue, 2> viewportClearValues = {viewportClear,
                                                       viewportDepthClear};

  vk::RenderingAttachmentInfo viewportColorAttachment = {
      .imageView = *viewportColorImageView,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = viewportClear};

  vk::RenderingAttachmentInfo viewportDepthAttachment = {
      .imageView = *viewportDepthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = viewportDepthClear};

  vk::RenderingInfo viewportRenderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = viewportExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &viewportColorAttachment,
      .pDepthAttachment = &viewportDepthAttachment};

  cmd.beginRendering(viewportRenderingInfo);

  updateLightBuffer(currentFrame);

  struct DrawEntry {
    MeshComponent *mc;
    TransformComponent *tc;
    uint32_t index;
  };
  std::vector<DrawEntry> drawList;

  Query<MeshComponent, TransformComponent> meshQuery(*world);
  meshQuery.for_each(
      [&](EntityId id, MeshComponent &mc, TransformComponent &tc) {
        if (!mc.mesh)
          return;
        drawList.push_back({&mc, &tc, static_cast<uint32_t>(drawList.size())});
      });

  if (!drawList.empty()) {
    ensureTransformBuffer(currentFrame, static_cast<uint32_t>(drawList.size()));

    for (auto &entry : drawList) {
      TransformUBO tu{.pos = glm::vec4(entry.tc->position, 0.0f),
                      .rotation = glm::vec4(entry.tc->rotation, 0.0f),
                      .scale = glm::vec4(entry.tc->scale, 0.0f)};
      vk::DeviceSize offset = transformStride * entry.index;
      memcpy(static_cast<uint8_t *>(transformBuffers[currentFrame].mapped) +
                 offset,
             &tu, sizeof(tu));
    }

    for (auto &entry : drawList) {
      auto *mesh = entry.mc->mesh.get();

      ShaderKey key;
      if (mesh->material != nullptr) {
        key = {mesh->material->vertexShader, mesh->material->fragmentShader};
      } else {
        key = {"sdr_default_model.vert", "sdr_default_model.frag"};
      }

      auto pipeline = getOrCreatePipeline(key);

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

      uint32_t dynamicOffset =
          static_cast<uint32_t>(transformStride * entry.index);

      cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                      static_cast<float>(viewportExtent.width),
                                      static_cast<float>(viewportExtent.height),
                                      0.0f, 1.0f));
      cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), viewportExtent));

      if (mesh->material != nullptr) {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0,
            {*descriptorSets[currentFrame], *mesh->material->descriptorSet},
            {dynamicOffset});
      } else {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0,
            {*descriptorSets[currentFrame]}, {dynamicOffset});
      }

      MaterialAsset *mat = mesh->material;
      MaterialPushConstants pc{
          .baseColorFactor =
              entry.mc->overrideColor.a > 0.0f
                  ? entry.mc->overrideColor
                  : (mat ? mat->baseColorFactor : glm::vec4(1.0f)),
          .metallicFactor = mat ? mat->metallicFactor : 1.0f,
          .roughnessFactor = mat ? mat->roughnessFactor : 1.0f,
          .parallaxStrength = mat ? mat->parallaxStrength : 0.0f};
      cmd.pushConstants(*pipelineLayout,
                        vk::ShaderStageFlagBits::eVertex |
                            vk::ShaderStageFlagBits::eFragment,
                        0, sizeof(pc), &pc);

      // Skip draw if mesh buffers haven't been loaded yet
      if (mesh->vertexBuffer == nullptr || mesh->indexBuffer == nullptr)
        continue;

      cmd.bindVertexBuffers(0, {*mesh->vertexBuffer}, {0});
      cmd.bindIndexBuffer(*mesh->indexBuffer, 0, vk::IndexType::eUint32);
      cmd.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
    }
  }

  cmd.endRendering();

  transition_image_layout(cmd, *viewportColorImage,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::AccessFlagBits2::eShaderRead,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eFragmentShader);

  // --- Pass 2: Copy viewport texture to swapchain ---

  if (editorMode) {
    transition_image_layout(cmd, imageIndex, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal, {},
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    std::array<vk::ClearValue, 1> imguiClearValues = {clearColor};

    vk::RenderingAttachmentInfo imguiColorAttachment = {
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingInfo imguiRenderingInfo = {
        .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &imguiColorAttachment};

    cmd.beginRendering(imguiRenderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd, VK_NULL_HANDLE);

    cmd.endRendering();

    transition_image_layout(cmd, imageIndex,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite, {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);
  } else {
    transition_image_layout(
        cmd, *viewportColorImage, vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eShaderRead,
        vk::AccessFlagBits2::eTransferRead,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::PipelineStageFlagBits2::eTransfer);

    transition_image_layout(cmd, imageIndex, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal, {},
                            vk::AccessFlagBits2::eTransferWrite,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eTransfer);

    std::array<vk::Offset3D, 2> srcOffsets = {
        vk::Offset3D{0, 0, 0},
        vk::Offset3D{static_cast<int32_t>(viewportExtent.width),
                     static_cast<int32_t>(viewportExtent.height), 1}};
    std::array<vk::Offset3D, 2> dstOffsets = {
        vk::Offset3D{0, 0, 0},
        vk::Offset3D{static_cast<int32_t>(swapChainExtent.width),
                     static_cast<int32_t>(swapChainExtent.height), 1}};

    vk::ImageBlit blitRegion = {
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .srcOffsets = srcOffsets,
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .dstOffsets = dstOffsets};

    cmd.blitImage(*viewportColorImage, vk::ImageLayout::eTransferSrcOptimal,
                  swapChainImages[imageIndex],
                  vk::ImageLayout::eTransferDstOptimal, blitRegion,
                  vk::Filter::eLinear);

    transition_image_layout(cmd, imageIndex,
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::AccessFlagBits2::eTransferWrite,
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::PipelineStageFlagBits2::eTransfer,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
    std::array<vk::ClearValue, 1> imguiClearValues = {clearColor};

    vk::RenderingAttachmentInfo imguiColorAttachment = {
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingInfo imguiRenderingInfo = {
        .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &imguiColorAttachment};

    cmd.beginRendering(imguiRenderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd, VK_NULL_HANDLE);

    cmd.endRendering();

    transition_image_layout(cmd, imageIndex,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite, {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);
  }

  cmd.end();
}

void VulkanRenderingContext::transition_image_layout(
    vk::raii::CommandBuffer &cmd, uint32_t imageIndex,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = swapChainImages[imageIndex],
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &barrier};
  cmd.pipelineBarrier2(dependencyInfo);
}

void VulkanRenderingContext::transition_image_layout(
    vk::raii::CommandBuffer &cmd, vk::Image image, vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &barrier};
  cmd.pipelineBarrier2(dependencyInfo);
}
