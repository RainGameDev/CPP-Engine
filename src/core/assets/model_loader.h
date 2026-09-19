#pragma once
#include "asset_loader.h"
#include "asset_manager.h"
#include "assets/obj_loader.h"
#include "imgui_impl_vulkan.h"
#include "rendering/mesh.h"
#include "rendering/ubo.h"
#include <cfloat>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

class ModelLoader : public AssetLoader<MeshAsset> {
  vk::raii::Device &device;
  vk::raii::PhysicalDevice &physicalDevice;
  vk::raii::CommandPool &commandPool;
  vk::raii::Queue &queue;
  AssetManager &assetManager;
  vk::DescriptorSet defaultMaterialSet;

  // Shared preview render-pass/pipeline, built once and reused for every
  // asset's thumbnail render, rather than per-asset.
  vk::raii::RenderPass previewRenderPass{nullptr};
  vk::raii::PipelineLayout previewPipelineLayout{nullptr};
  vk::raii::Pipeline previewPipeline{nullptr};

  // Dedicated camera/transform/lights set for thumbnail renders, so previews
  // are framed independently of the live scene camera.
  struct PreviewCamera {
    vk::raii::Buffer camBuffer{nullptr};
    vk::raii::DeviceMemory camMemory{nullptr};
    void *camMapped = nullptr;
    vk::raii::Buffer transformBuffer{nullptr};
    vk::raii::DeviceMemory transformMemory{nullptr};
    void *transformMapped = nullptr;
    vk::raii::Buffer lightBuffer{nullptr};
    vk::raii::DeviceMemory lightMemory{nullptr};
    void *lightMapped = nullptr;
    vk::raii::DescriptorPool pool{nullptr};
    vk::raii::DescriptorSets sets{nullptr};

    vk::raii::Buffer shadowBuffer{nullptr};
    vk::raii::DeviceMemory shadowMemory{nullptr};
    void *shadowMapped = nullptr;
    vk::raii::Sampler dummySampler{nullptr};
    vk::raii::Image dummyImage{nullptr};
    vk::raii::DeviceMemory dummyMemory{nullptr};
    vk::raii::ImageView dummyView{nullptr};

    VkDescriptorSet set = VK_NULL_HANDLE;
  } previewCamera;

public:
  ModelLoader(vk::raii::Device &device,
              vk::raii::PhysicalDevice &physicalDevice,
              vk::raii::CommandPool &commandPool, vk::raii::Queue &queue,
              AssetManager &assetManager,
              vk::DescriptorSetLayout perFrameSetLayout,
              vk::DescriptorSetLayout materialSetLayout,
              vk::DescriptorSet defaultMaterialSet)
      : device(device), physicalDevice(physicalDevice),
        commandPool(commandPool), queue(queue), assetManager(assetManager),
        defaultMaterialSet(defaultMaterialSet) {
    createPreviewRenderPass();
    createPreviewPipeline(perFrameSetLayout, materialSetLayout);
    createPreviewCamera(perFrameSetLayout);
  }

  std::vector<std::string> extensions() const override {
    return {".obj", ".gltf", ".glb"};
  }

  MeshAsset loadAsset(const std::string &path) override {
    ObjData data = loadObj(path);

    Mesh mesh{};
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.vertexCount = static_cast<uint32_t>(data.vertices.size());
    if (auto *materials = assetManager.getLoader<MaterialAsset>()) {
      if (auto *mat = materials->tryGetAsset("brick.mat"))
        mesh.material = mat;
    }

    // Vertex buffer
    vk::DeviceSize vertexBufferSize = sizeof(Vertex) * data.vertices.size();
    vk::raii::Buffer vertexStagingBuffer{nullptr};
    vk::raii::DeviceMemory vertexStagingMemory{nullptr};
    createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 vertexStagingBuffer, vertexStagingMemory);

    void *mapped = vertexStagingMemory.mapMemory(0, vertexBufferSize);
    memcpy(mapped, data.vertices.data(), vertexBufferSize);
    vertexStagingMemory.unmapMemory();

    createBuffer(vertexBufferSize,
                 vk::BufferUsageFlagBits::eVertexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst |
                     vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.vertexBuffer,
                 mesh.vertexMemory);

    copyBuffer(vertexStagingBuffer, mesh.vertexBuffer, vertexBufferSize);

    // Index buffer
    vk::DeviceSize indexBufferSize = sizeof(uint32_t) * data.indices.size();
    vk::raii::Buffer indexStagingBuffer{nullptr};
    vk::raii::DeviceMemory indexStagingMemory{nullptr};
    createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 indexStagingBuffer, indexStagingMemory);

    mapped = indexStagingMemory.mapMemory(0, indexBufferSize);
    memcpy(mapped, data.indices.data(), indexBufferSize);
    indexStagingMemory.unmapMemory();

    createBuffer(indexBufferSize,
                 vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst |
                     vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.indexBuffer,
                 mesh.indexMemory);

    copyBuffer(indexStagingBuffer, mesh.indexBuffer, indexBufferSize);

    MeshAsset asset{};
    asset.mesh = std::move(mesh);

    glm::vec3 minV{FLT_MAX};
    glm::vec3 maxV{-FLT_MAX};
    for (const auto &v : data.vertices) {
      minV = glm::min(minV, v.pos);
      maxV = glm::max(maxV, v.pos);
    }
    asset.previewMin = minV;
    asset.previewMax = maxV;

    createPreviewTarget(asset);
    renderPreview(asset);

    return asset;
  }

  /// Makes an independent copy of a loaded mesh with fresh GPU buffers, so
  /// scene entities can hold their own geometry without emptying the asset
  /// that the preview thumbnails are rendered from.
  Mesh duplicateMesh(const Mesh &mesh) {
    Mesh clone{};
    clone.vertexCount = mesh.vertexCount;
    clone.indexCount = mesh.indexCount;
    clone.material = mesh.material;
    if (mesh.vertexCount > 0) {
      vk::DeviceSize vbSize = sizeof(Vertex) * mesh.vertexCount;
      createBuffer(vbSize,
                   vk::BufferUsageFlagBits::eVertexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal, clone.vertexBuffer,
                   clone.vertexMemory);
      copyBuffer(mesh.vertexBuffer, clone.vertexBuffer, vbSize);
    }
    if (mesh.indexCount > 0) {
      vk::DeviceSize ibSize = sizeof(uint32_t) * mesh.indexCount;
      createBuffer(ibSize,
                   vk::BufferUsageFlagBits::eIndexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal, clone.indexBuffer,
                   clone.indexMemory);
      copyBuffer(mesh.indexBuffer, clone.indexBuffer, ibSize);
    }
    return clone;
  }

  /// Re-renders an asset's thumbnail after its preview orientation changed.
  void refreshPreview(const std::string &name) {
    auto it = assets.find(name);
    if (it == assets.end())
      return;
    MeshAsset &asset = it->second;
    device.waitIdle();
    renderPreview(asset);
  }

private:
  // ---------------------------------------------------------------------
  // Shared preview render pass / pipeline (built once)
  // ---------------------------------------------------------------------

  void createPreviewRenderPass() {
    vk::AttachmentDescription colorAttachment{
        .format = MeshAsset::kPreviewColorFormat,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    vk::AttachmentDescription depthAttachment{
        .format = MeshAsset::kPreviewDepthFormat,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::AttachmentReference colorRef{
        .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
    vk::AttachmentReference depthRef{
        .attachment = 1,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpass{.pipelineBindPoint =
                                       vk::PipelineBindPoint::eGraphics,
                                   .colorAttachmentCount = 1,
                                   .pColorAttachments = &colorRef,
                                   .pDepthStencilAttachment = &depthRef};

    std::array<vk::SubpassDependency, 2> dependencies{{
        {.srcSubpass = VK_SUBPASS_EXTERNAL,
         .dstSubpass = 0,
         .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
         .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                         vk::PipelineStageFlagBits::eEarlyFragmentTests,
         .srcAccessMask = vk::AccessFlagBits::eShaderRead,
         .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                          vk::AccessFlagBits::eDepthStencilAttachmentWrite},
        {.srcSubpass = 0,
         .dstSubpass = VK_SUBPASS_EXTERNAL,
         .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
         .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
         .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
         .dstAccessMask = vk::AccessFlagBits::eShaderRead},
    }};

    std::array<vk::AttachmentDescription, 2> attachments{colorAttachment,
                                                         depthAttachment};

    vk::RenderPassCreateInfo rpInfo{
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<uint32_t>(dependencies.size()),
        .pDependencies = dependencies.data()};

    previewRenderPass = vk::raii::RenderPass(device, rpInfo);
  }

  static std::vector<char> readSpirv(const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
      throw std::runtime_error("failed to open shader file: " + path);
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
  }

  vk::raii::ShaderModule loadShaderModule(const std::string &path) {
    auto code = readSpirv(path);
    vk::ShaderModuleCreateInfo info{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};
    return vk::raii::ShaderModule(device, info);
  }

  void createPreviewPipeline(vk::DescriptorSetLayout perFrameSetLayout,
                             vk::DescriptorSetLayout materialSetLayout) {
    auto vertModule =
        loadShaderModule("assets/shaders/pbr_model_standard.vert.spv");
    auto fragModule =
        loadShaderModule("assets/shaders/pbr_model_standard.frag.spv");

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{{
        {.stage = vk::ShaderStageFlagBits::eVertex,
         .module = *vertModule,
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment,
         .module = *fragModule,
         .pName = "main"},
    }};

    auto bindingDesc = Vertex::getBindingDescription();
    auto attribDescs = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInput{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attribDescs.size()),
        .pVertexAttributeDescriptions = attribDescs.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

    std::array<vk::DescriptorSetLayout, 2> setLayouts{perFrameSetLayout,
                                                      materialSetLayout};
    vk::PushConstantRange pushConstant{.stageFlags =
                                           vk::ShaderStageFlagBits::eVertex |
                                           vk::ShaderStageFlagBits::eFragment,
                                       .offset = 0,
                                       .size = sizeof(MaterialPushConstants)};

    vk::PipelineLayoutCreateInfo layoutInfo{
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant};
    previewPipelineLayout = vk::raii::PipelineLayout(device, layoutInfo);

    vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport,
                                        vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = 2, .pDynamicStates = dynamicStates};

    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *previewPipelineLayout,
        .renderPass = *previewRenderPass,
        .subpass = 0};

    previewPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
  }

  void createPreviewCamera(vk::DescriptorSetLayout perFrameSetLayout) {
    // Buffers host-visible + coherent so they can be written directly.
    auto makeBuffer = [&](vk::raii::Buffer &buffer,
                          vk::raii::DeviceMemory &memory, void *&mapped,
                          vk::DeviceSize size) {
      createBuffer(size, vk::BufferUsageFlagBits::eUniformBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent,
                   buffer, memory);
      mapped = memory.mapMemory(0, size);
    };
    makeBuffer(previewCamera.camBuffer, previewCamera.camMemory,
               previewCamera.camMapped, sizeof(UniformBufferObject));
    makeBuffer(previewCamera.transformBuffer, previewCamera.transformMemory,
               previewCamera.transformMapped, sizeof(TransformUBO));
    makeBuffer(previewCamera.lightBuffer, previewCamera.lightMemory,
               previewCamera.lightMapped, sizeof(LightsUBO));

    makeBuffer(previewCamera.shadowBuffer, previewCamera.shadowMemory,
               previewCamera.shadowMapped, sizeof(ShadowUBO));
    ShadowUBO ident{};
    ident.lightViewProj = glm::mat4(1.0f);
    memcpy(previewCamera.shadowMapped, &ident, sizeof(ident));

    previewCamera.dummySampler = vk::raii::Sampler(
        device, {.magFilter = vk::Filter::eLinear,
                 .minFilter = vk::Filter::eLinear,
                 .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                 .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                 .addressModeW = vk::SamplerAddressMode::eClampToEdge});
    previewCamera.dummyImage =
        vk::raii::Image(device, {.imageType = vk::ImageType::e2D,
                                 .format = vk::Format::eR8G8B8A8Unorm,
                                 .extent = {1, 1, 1},
                                 .mipLevels = 1,
                                 .arrayLayers = 1,
                                 .samples = vk::SampleCountFlagBits::e1,
                                 .tiling = vk::ImageTiling::eOptimal,
                                 .usage = vk::ImageUsageFlagBits::eSampled,
                                 .initialLayout = vk::ImageLayout::eUndefined});
    auto m = previewCamera.dummyImage.getMemoryRequirements();
    previewCamera.dummyMemory = vk::raii::DeviceMemory(
        device,
        {.allocationSize = m.size,
         .memoryTypeIndex = findMemoryType(
             m.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)});
    previewCamera.dummyImage.bindMemory(*previewCamera.dummyMemory, 0);
    previewCamera.dummyView = vk::raii::ImageView(
        device,
        {.image = *previewCamera.dummyImage,
         .viewType = vk::ImageViewType::e2D,
         .format = vk::Format::eR8G8B8A8Unorm,
         .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                              .levelCount = 1,
                              .layerCount = 1}});

    {
      vk::CommandBufferBeginInfo bi{
          .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
      auto cb = vk::raii::CommandBuffers(
          device, {.commandPool = *commandPool,
                   .level = vk::CommandBufferLevel::ePrimary,
                   .commandBufferCount = 1});
      auto &c = cb.front();
      c.begin(bi);
      vk::ImageMemoryBarrier2 b{
          .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
          .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
          .oldLayout = vk::ImageLayout::eUndefined,
          .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
          .image = *previewCamera.dummyImage,
          .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                               .levelCount = 1,
                               .layerCount = 1}};
      c.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &b});
      c.end();
      vk::CommandBuffer raw = *c;
      vk::SubmitInfo s{.commandBufferCount = 1, .pCommandBuffers = &raw};
      queue.submit(s, nullptr);
      queue.waitIdle();
    }

    std::array<vk::DescriptorPoolSize, 3> poolSizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 3},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1}};

    previewCamera.pool = device.createDescriptorPool(
        {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
         .maxSets = 1,
         .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
         .pPoolSizes = poolSizes.data()});

    vk::DescriptorSetAllocateInfo alloc{.descriptorPool = *previewCamera.pool,
                                        .descriptorSetCount = 1,
                                        .pSetLayouts = &perFrameSetLayout};
    previewCamera.sets = device.allocateDescriptorSets(alloc);
    previewCamera.set = *previewCamera.sets.front();

    vk::DescriptorBufferInfo camInfo{.buffer = *previewCamera.camBuffer,
                                     .offset = 0,
                                     .range = sizeof(UniformBufferObject)};
    vk::DescriptorBufferInfo transformInfo{.buffer =
                                               *previewCamera.transformBuffer,
                                           .offset = 0,
                                           .range = sizeof(TransformUBO)};
    vk::DescriptorBufferInfo lightInfo{.buffer = *previewCamera.lightBuffer,
                                       .offset = 0,
                                       .range = sizeof(LightsUBO)};
    vk::DescriptorBufferInfo shadowInfo{.buffer = *previewCamera.shadowBuffer,
                                        .offset = 0,
                                        .range = sizeof(ShadowUBO)};
    vk::DescriptorImageInfo dummyImageInfo{
        .sampler = *previewCamera.dummySampler,
        .imageView = *previewCamera.dummyView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    std::array<vk::WriteDescriptorSet, 5> writes = {{
        {.dstSet = previewCamera.set,
         .dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &camInfo},
        {.dstSet = previewCamera.set,
         .dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
         .pBufferInfo = &transformInfo},
        {.dstSet = previewCamera.set,
         .dstBinding = 2,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &lightInfo},
        {.dstSet = previewCamera.set,
         .dstBinding = 3,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &shadowInfo},
        {.dstSet = previewCamera.set,
         .dstBinding = 4,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &dummyImageInfo},
    }};
    device.updateDescriptorSets(writes, nullptr);

    LightsUBO lights{};
    lights.count = 1;
    lights.lights[0].positionOrDirection =
        glm::vec4(glm::normalize(glm::vec3(0.5f, 0.8f, 0.6f)), 0.0f);
    lights.lights[0].direction =
        glm::vec4(-glm::normalize(glm::vec3(0.5f, 0.8f, 0.6f)), 0.0f);
    lights.lights[0].colorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 1.5f);
    memcpy(previewCamera.lightMapped, &lights, sizeof(lights));
  }

  // ---------------------------------------------------------------------
  // Per-asset preview target
  // ---------------------------------------------------------------------

  void createPreviewTarget(MeshAsset &asset) {
    const uint32_t sz = MeshAsset::kPreviewSize;

    // Color image
    vk::ImageCreateInfo colorInfo{.imageType = vk::ImageType::e2D,
                                  .format = MeshAsset::kPreviewColorFormat,
                                  .extent = {sz, sz, 1},
                                  .mipLevels = 1,
                                  .arrayLayers = 1,
                                  .samples = vk::SampleCountFlagBits::e1,
                                  .tiling = vk::ImageTiling::eOptimal,
                                  .usage =
                                      vk::ImageUsageFlagBits::eColorAttachment |
                                      vk::ImageUsageFlagBits::eSampled,
                                  .sharingMode = vk::SharingMode::eExclusive,
                                  .initialLayout = vk::ImageLayout::eUndefined};
    asset.previewImage = vk::raii::Image(device, colorInfo);

    auto colorMem = asset.previewImage.getMemoryRequirements();
    vk::MemoryAllocateInfo colorAlloc{
        .allocationSize = colorMem.size,
        .memoryTypeIndex = findMemoryType(
            colorMem.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
    asset.previewMemory = vk::raii::DeviceMemory(device, colorAlloc);
    asset.previewImage.bindMemory(*asset.previewMemory, 0);

    vk::ImageViewCreateInfo colorViewInfo{
        .image = *asset.previewImage,
        .viewType = vk::ImageViewType::e2D,
        .format = MeshAsset::kPreviewColorFormat,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .levelCount = 1,
                             .layerCount = 1}};
    asset.previewView = vk::raii::ImageView(device, colorViewInfo);

    // Depth image
    vk::ImageCreateInfo depthInfo{
        .imageType = vk::ImageType::e2D,
        .format = MeshAsset::kPreviewDepthFormat,
        .extent = {sz, sz, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined};
    asset.previewDepthImage = vk::raii::Image(device, depthInfo);

    auto depthMem = asset.previewDepthImage.getMemoryRequirements();
    vk::MemoryAllocateInfo depthAlloc{
        .allocationSize = depthMem.size,
        .memoryTypeIndex = findMemoryType(
            depthMem.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};
    asset.previewDepthMemory = vk::raii::DeviceMemory(device, depthAlloc);
    asset.previewDepthImage.bindMemory(*asset.previewDepthMemory, 0);

    vk::ImageViewCreateInfo depthViewInfo{
        .image = *asset.previewDepthImage,
        .viewType = vk::ImageViewType::e2D,
        .format = MeshAsset::kPreviewDepthFormat,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                             .levelCount = 1,
                             .layerCount = 1}};
    asset.previewDepthView = vk::raii::ImageView(device, depthViewInfo);

    // Sampler
    asset.previewSampler = vk::raii::Sampler(
        device, {.magFilter = vk::Filter::eLinear,
                 .minFilter = vk::Filter::eLinear,
                 .mipmapMode = vk::SamplerMipmapMode::eNearest,
                 .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                 .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                 .addressModeW = vk::SamplerAddressMode::eClampToEdge});

    // Framebuffer
    std::array<vk::ImageView, 2> fbAttachments{*asset.previewView,
                                               *asset.previewDepthView};
    vk::FramebufferCreateInfo fbInfo{
        .renderPass = *previewRenderPass,
        .attachmentCount = static_cast<uint32_t>(fbAttachments.size()),
        .pAttachments = fbAttachments.data(),
        .width = sz,
        .height = sz,
        .layers = 1};
    asset.previewFramebuffer = vk::raii::Framebuffer(device, fbInfo);
  }

  void renderPreview(MeshAsset &asset) {
    const uint32_t sz = MeshAsset::kPreviewSize;

    // Bounding box, purely for framing the camera — the mesh's own
    // vertex data is never shifted, so there's no mismatch between where
    // the geometry actually is and where the camera looks.
    glm::vec3 minV = asset.previewMin;
    glm::vec3 maxV = asset.previewMax;
    glm::vec3 center = (minV + maxV) * 0.5f;
    glm::vec3 extent = maxV - minV;
    float maxDim = std::max({extent.x, extent.y, extent.z});
    if (maxDim < 1e-6f)
      maxDim = 1.0f;

    float dist = maxDim * 2.0f;
    glm::vec3 eye = center + glm::vec3(dist * 0.7f, dist * 0.5f, dist * 0.7f);
    glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));

    float half = maxDim * 0.55f;
    glm::mat4 proj =
        glm::orthoZO(-half, half, -half, half, 0.01f, dist + maxDim * 2.0f);
    // Vulkan Y-flip
    proj[1][1] *= -1;

    UniformBufferObject cubo{};
    cubo.view = view;
    cubo.proj = proj;
    cubo.pos = glm::vec4(eye, 1.0f);
    memcpy(previewCamera.camMapped, &cubo, sizeof(cubo));

    // Per-asset orientation applied as a mesh transform (model space).
    TransformUBO transform{};
    transform.model =
        glm::rotate(glm::mat4(1.0f), glm::radians(asset.previewPitch),
                    glm::vec3(1.0f, 0.0f, 0.0f));
    transform.model =
        glm::rotate(transform.model, glm::radians(asset.previewYaw),
                    glm::vec3(0.0f, 1.0f, 0.0f));
    memcpy(previewCamera.transformMapped, &transform, sizeof(transform));

    MaterialPushConstants pc{};
    pc.baseColorFactor = glm::vec4(1.0f);
    pc.metallicFactor = 0.0f;
    pc.roughnessFactor = 1.0f;
    pc.parallaxStrength = 0.0f;

    auto cmdBufs = vk::raii::CommandBuffers(
        device, {.commandPool = *commandPool,
                 .level = vk::CommandBufferLevel::ePrimary,
                 .commandBufferCount = 1});
    auto cmd = std::move(cmdBufs.front());
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    std::array<vk::ClearValue, 2> clearValues{
        vk::ClearColorValue{std::array{0.1f, 0.1f, 0.1f, 1.0f}},
        vk::ClearDepthStencilValue{1.0f, 0}};

    vk::RenderPassBeginInfo rpInfo{
        .renderPass = *previewRenderPass,
        .framebuffer = *asset.previewFramebuffer,
        .renderArea = {{0, 0}, {sz, sz}},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()};
    cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *previewPipeline);

    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(sz),
                                    static_cast<float>(sz), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {sz, sz}});

    cmd.pushConstants<MaterialPushConstants>(
        *previewPipelineLayout,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, pc);

    // Bind descriptor sets
    if (asset.mesh.material) {
      cmd.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, *previewPipelineLayout, 0,
          {previewCamera.set, *asset.mesh.material->descriptorSet}, {0});
    } else {
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                             *previewPipelineLayout, 0,
                             {previewCamera.set, defaultMaterialSet}, {0});
    }

    cmd.bindVertexBuffers(0, {*asset.mesh.vertexBuffer}, {0});
    cmd.bindIndexBuffer(*asset.mesh.indexBuffer, 0, vk::IndexType::eUint32);
    cmd.drawIndexed(asset.mesh.indexCount, 1, 0, 0, 0);

    cmd.endRenderPass();

    cmd.end();

    vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &*cmd};
    queue.submit(submit, nullptr);
    queue.waitIdle();

    // Keep the same ImGui descriptor set for the asset's whole lifetime.
    // Re-uploading into the same image is safe because the caller waits for
    // the device to be idle before re-rendering, and the descriptor set stays
    // valid so the current frame's already-recorded ImGui draw still binds a
    // live set (no mid-frame RemoveTexture).
    if (asset.previewTexture == VK_NULL_HANDLE) {
      asset.previewTexture =
          ImGui_ImplVulkan_AddTexture(*asset.previewSampler, *asset.previewView,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
  }

  // ---------------------------------------------------------------------
  // Existing helpers
  // ---------------------------------------------------------------------

  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory) {
    vk::BufferCreateInfo bufferInfo{.size = size,
                                    .usage = usage,
                                    .sharingMode = vk::SharingMode::eExclusive};
    buffer = vk::raii::Buffer(device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits, properties)};

    bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
  }

  void copyBuffer(const vk::raii::Buffer &srcBuffer,
                  vk::raii::Buffer &dstBuffer, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1};

    auto commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    auto copyCommandBuffer = std::move(commandBuffers.front());

    copyCommandBuffer.begin(
        {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
    copyCommandBuffer.copyBuffer(*srcBuffer, *dstBuffer, copyRegion);

    copyCommandBuffer.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*copyCommandBuffer};

    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
  }

  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties) {
    auto memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
              properties)
        return i;
    }
    throw std::runtime_error("failed to find suitable memory type");
  }
};
