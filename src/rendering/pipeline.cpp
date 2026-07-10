
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include "rendering/vertex.h"
#include "rendering/vulkan_application.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

void VulkanApplication::createGraphicsPipeline() {
  auto fragCode = readFile("src/assets/shaders/sdr_default_model.frag.spv");
  auto vertCode = readFile("src/assets/shaders/sdr_default_model.vert.spv");

  vk::raii::ShaderModule fragShader = createShaderModule(fragCode);
  vk::raii::ShaderModule vertShader = createShaderModule(vertCode);

  vk::PipelineShaderStageCreateInfo vertStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = vertShader,
      .pName = "main"};
  vk::PipelineShaderStageCreateInfo fragStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = fragShader,
      .pName = "main"};

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo,
                                                      fragStageInfo};

  vk::Viewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(swapChainExtent.width),
                        static_cast<float>(swapChainExtent.height),
                        0.0f,
                        1.0f};

  vk::Rect2D scissor{vk::Offset2D{0, 0}, swapChainExtent};

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};

  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                    .pViewports = &viewport,
                                                    .scissorCount = 1,
                                                    .pScissors = &scissor};

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
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1,
                                                  .pSetLayouts =
                                                      &*descriptorSetLayout,
                                                  .pushConstantRangeCount = 0};

  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList};

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
           .pStages = shaderStages,
           .pVertexInputState = &vertexInputInfo,
           .pInputAssemblyState = &inputAssembly,
           .pViewportState = &viewportState,
           .pRasterizationState = &rasterizer,
           .pMultisampleState = &multisampling,
           .pColorBlendState = &colorBlending,
           .pDynamicState = &dynamicState,
           .layout = pipelineLayout,
           .renderPass = nullptr},
          {.colorAttachmentCount = 1,
           .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

  graphicsPipeline = vk::raii::Pipeline(
      device, nullptr,
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

vk::raii::ShaderModule
VulkanApplication::createShaderModule(const std::vector<char> &code) const {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};
  vk::raii::ShaderModule shaderModule{device, createInfo};
  return shaderModule;
}

std::vector<char> VulkanApplication::readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + filename);
  }
  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  return buffer;
}
