#include "rendering/vulkan_render_context.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

void VulkanRenderingContext::createSyncObjects() {
  presentCompleteSemaphores.clear();
  renderFinishedSemaphores.clear();
  inFlightFences.clear();
  uint32_t imageCount = static_cast<uint32_t>(swapChainImages.size());
  for (uint32_t i = 0; i < imageCount; i++) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }
  for (uint32_t i = 0; i < maxConcurrentFrames; i++) {
    inFlightFences.push_back(
        vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled}));
  }
}

void VulkanRenderingContext::drawFrame() {
  auto fenceResult = device.waitForFences(*inFlightFences[currentFrame],
                                          vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }

  auto [result, imageIndex] = swapChain.acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphores[currentFrame], nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR) {
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    return;
  }

  device.resetFences(*inFlightFences[currentFrame]);

  recordCommandBuffer(imageIndex, currentFrame % maxConcurrentFrames);

  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphores[currentFrame],
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]};
  queue.submit(submitInfo, *inFlightFences[currentFrame]);

  const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                          .pWaitSemaphores =
                                              &*renderFinishedSemaphores[imageIndex],
                                          .swapchainCount = 1,
                                          .pSwapchains = &*swapChain,
                                          .pImageIndices = &imageIndex};

  result = queue.presentKHR(presentInfoKHR);

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR || framebufferResized) {
    framebufferResized = false;
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    currentFrame = (currentFrame + 1) % maxConcurrentFrames;
    return;
  }

  currentFrame = (currentFrame + 1) % maxConcurrentFrames;

  switch (result) {
  case vk::Result::eSuccess:
    break;
  case vk::Result::eSuboptimalKHR:
    std::cout
        << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
    createSwapChain();
    createImageViews();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    break;
  default:
    break;
  }
}
