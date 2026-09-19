
#include "assets/material_loader.h"
#include "assets/shader_loader.h"
#include "rendering/vertex.h"
#include "rendering/vulkan_render_context.h"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <iostream>
#include <print>
#include <stdexcept>
#include <vector>

void VulkanRenderingContext::createGraphicsPipelineLayout() {
  std::array<vk::DescriptorSetLayout, 2> setLayouts = {descriptorSetLayout,
                                                       materialSetLayout};

  vk::PushConstantRange pushRange{.stageFlags =
                                      vk::ShaderStageFlagBits::eVertex |
                                      vk::ShaderStageFlagBits::eFragment,
                                  .offset = 0,
                                  .size = sizeof(MaterialPushConstants)};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 2,
      .pSetLayouts = setLayouts.data(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushRange};

  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
}

vk::Pipeline VulkanRenderingContext::getOrCreatePipeline(const ShaderKey &key) {
  if (auto it = graphicsPipelines.find(key); it != graphicsPipelines.end()) {
    return **it->second;
  }
  createPipelineForKey(key);
  return **graphicsPipelines[key];
}

void VulkanRenderingContext::createPipelineForKey(const ShaderKey &key) {
  AssetManager &assetManager = *world->get_resource<AssetManager>();
  auto *shaders = assetManager.getLoader<ShaderAsset>();

  std::cout << key.fragment + " " + key.vertex << std::endl;

  auto &vertModule = shaders->getAsset(key.vertex).module;
  auto &fragModule = shaders->getAsset(key.fragment).module;

  vk::PipelineShaderStageCreateInfo vertStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = vertModule,
      .pName = "main"};
  vk::PipelineShaderStageCreateInfo fragStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = fragModule,
      .pName = "main"};

  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
      vertStageInfo, fragStageInfo};

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};

  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                    .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f};

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::False,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList};

  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False};

  auto bindingDesc = Vertex::getBindingDescription();
  auto attribDescs = Vertex::getAttributeDescriptions();

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDesc,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attribDescs.size()),
      .pVertexAttributeDescriptions = attribDescs.data()};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {
          {.stageCount = 2,
           .pStages = shaderStages.data(),
           .pVertexInputState = &vertexInputInfo,
           .pInputAssemblyState = &inputAssembly,
           .pViewportState = &viewportState,
           .pRasterizationState = &rasterizer,
           .pMultisampleState = &multisampling,
           .pDepthStencilState = &depthStencil,
           .pColorBlendState = &colorBlending,
           .pDynamicState = &dynamicState,
           .layout = pipelineLayout,
           .renderPass = nullptr},
          {.colorAttachmentCount = 1,
           .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
           .depthAttachmentFormat = depthFormat}};

  auto pipeline = vk::raii::Pipeline(
      device, nullptr,
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

  graphicsPipelines[key] =
      std::make_unique<vk::raii::Pipeline>(std::move(pipeline));
}
void VulkanRenderingContext::createShadowPipeline() {
  AssetManager &assetManager = *world->get_resource<AssetManager>();
  auto *shaders = assetManager.getLoader<ShaderAsset>();
  auto &vertModule = shaders->getAsset("shadow_depth.vert").module;

  vk::PipelineShaderStageCreateInfo vertStage{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = vertModule,
      .pName = "main"};

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor,
                                                 vk::DynamicState::eDepthBias};

  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};
  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                    .scissorCount = 1};
  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::True,
      .lineWidth = 1.0f};
  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1};
  vk::PipelineColorBlendStateCreateInfo colorBlending{.attachmentCount = 0};
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList};
  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLessOrEqual};

  auto bindingDesc = Vertex::getBindingDescription();
  auto attribDescs = Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDesc,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attribDescs.size()),
      .pVertexAttributeDescriptions = attribDescs.data()};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      chain = {
          {.stageCount = 1,
           .pStages = &vertStage,
           .pVertexInputState = &vertexInputInfo,
           .pInputAssemblyState = &inputAssembly,
           .pViewportState = &viewportState,
           .pRasterizationState = &rasterizer,
           .pMultisampleState = &multisampling,
           .pDepthStencilState = &depthStencil,
           .pColorBlendState = &colorBlending,
           .pDynamicState = &dynamicState,
           .layout = pipelineLayout},
          {.colorAttachmentCount = 0, .depthAttachmentFormat = depthFormat}};

  shadowPipeline = vk::raii::Pipeline(
      device, nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());
}
