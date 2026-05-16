#include "example_postprocess.h"
#include "vkUtils.h"
#include <android/log.h>
#include <vector>
#include <array>
#include <cstring>
#include <cmath>

#define TAG "Vulkan-PostProcess"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))

static uint32_t findMemoryTypePost(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

struct PostVertex {
    float position[2];
    float uv[2];
};

struct GlobalUniform3D {
    float ProjView[16];
    float camera[4];
    LightData lights[3];
};

IExample* createPostProcessExample() {
    return new PostProcessExample();
}

void PostProcessExample::createOffscreenResources(VulkanInstance& inst, VulkanSwapchain& chain) {
    offFormat = VK_FORMAT_R8G8B8A8_UNORM;
    offDepthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = chain.swapChainExtent.width;
    imgInfo.extent.height = chain.swapChainExtent.height;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = offFormat;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.flags = 0;
    if (vkCreateImage(inst.device, &imgInfo, nullptr, &offImage) != VK_SUCCESS) return;
    VkMemoryRequirements offReq;
    vkGetImageMemoryRequirements(inst.device, offImage, &offReq);
    VkMemoryAllocateInfo offAlloc{};
    offAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    offAlloc.allocationSize = offReq.size;
    offAlloc.memoryTypeIndex = findMemoryTypePost(inst.physicalDevice, offReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(inst.device, &offAlloc, nullptr, &offMemory) != VK_SUCCESS) return;
    vkBindImageMemory(inst.device, offImage, offMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = offImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = offFormat;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(inst.device, &viewInfo, nullptr, &offView);

    VkImageCreateInfo depthInfo = imgInfo;
    depthInfo.format = offDepthFormat;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vkCreateImage(inst.device, &depthInfo, nullptr, &offDepthImage);
    VkMemoryRequirements offDepthReq;
    vkGetImageMemoryRequirements(inst.device, offDepthImage, &offDepthReq);
    VkMemoryAllocateInfo offDepthAlloc{};
    offDepthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    offDepthAlloc.allocationSize = offDepthReq.size;
    offDepthAlloc.memoryTypeIndex = findMemoryTypePost(inst.physicalDevice, offDepthReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(inst.device, &offDepthAlloc, nullptr, &offDepthMemory);
    vkBindImageMemory(inst.device, offDepthImage, offDepthMemory, 0);

    VkImageViewCreateInfo depthViewInfo = viewInfo;
    depthViewInfo.image = offDepthImage;
    depthViewInfo.format = offDepthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkCreateImageView(inst.device, &depthViewInfo, nullptr, &offDepthView);

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = offFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = offDepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[2] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = atts;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;
    vkCreateRenderPass(inst.device, &rpInfo, nullptr, &offRenderPass);

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = offRenderPass;
    fbInfo.attachmentCount = 2;
    VkImageView fbAtts[2] = {offView, offDepthView};
    fbInfo.pAttachments = fbAtts;
    fbInfo.width = chain.swapChainExtent.width;
    fbInfo.height = chain.swapChainExtent.height;
    fbInfo.layers = 1;
    vkCreateFramebuffer(inst.device, &fbInfo, nullptr, &offFramebuffer);

    VkSamplerCreateInfo sampInfo{};
    sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;
    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.anisotropyEnable = VK_FALSE;
    sampInfo.maxAnisotropy = 1.0f;
    sampInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampInfo.unnormalizedCoordinates = VK_FALSE;
    sampInfo.compareEnable = VK_FALSE;
    sampInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampInfo.mipLodBias = 0.0f;
    sampInfo.minLod = 0.0f;
    sampInfo.maxLod = 0.0f;
    vkCreateSampler(inst.device, &sampInfo, nullptr, &offSampler);
}

void PostProcessExample::createBloomResources(VulkanInstance& inst, VulkanSwapchain& chain) {
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = chain.swapChainExtent.width;
    imgInfo.extent.height = chain.swapChainExtent.height;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = offFormat;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.flags = 0;

    for (int j = 0; j < 2; j++) {
        VkImage* img = (j == 0) ? &bloomImage1 : &bloomImage2;
        VkDeviceMemory* mem = (j == 0) ? &bloomMemory1 : &bloomMemory2;
        VkImageView* view = (j == 0) ? &bloomView1 : &bloomView2;
        vkCreateImage(inst.device, &imgInfo, nullptr, img);
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(inst.device, *img, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = findMemoryTypePost(inst.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(inst.device, &alloc, nullptr, mem);
        vkBindImageMemory(inst.device, *img, *mem, 0);
        VkImageViewCreateInfo vinfo{};
        vinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vinfo.image = *img;
        vinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vinfo.format = offFormat;
        vinfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        vinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vinfo.subresourceRange.baseMipLevel = 0;
        vinfo.subresourceRange.levelCount = 1;
        vinfo.subresourceRange.baseArrayLayer = 0;
        vinfo.subresourceRange.layerCount = 1;
        vkCreateImageView(inst.device, &vinfo, nullptr, view);
    }

    {
        VkAttachmentDescription att{};
        att.format = offFormat;
        att.samples = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &ref;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp.attachmentCount = 1;
        rp.pAttachments = &att;
        rp.subpassCount = 1;
        rp.pSubpasses = &sub;
        rp.dependencyCount = 1;
        rp.pDependencies = &dep;
        vkCreateRenderPass(inst.device, &rp, nullptr, &bloomRenderPass);
    }

    auto createFB = [&](VkFramebuffer& fb, VkImageView v) {
        VkFramebufferCreateInfo fbi{};
        fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = bloomRenderPass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &v;
        fbi.width = chain.swapChainExtent.width;
        fbi.height = chain.swapChainExtent.height;
        fbi.layers = 1;
        vkCreateFramebuffer(inst.device, &fbi, nullptr, &fb);
    };
    createFB(bloomFB1, bloomView1);
    createFB(bloomFB2, bloomView2);

    {
        VkDescriptorPoolSize poolSizes[1];
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 10, 1, poolSizes};
        vkCreateDescriptorPool(inst.device, &poolInfo, nullptr, &bloomPool);
    }
}

void PostProcessExample::createSceneResources(VulkanInstance& inst, VulkanSwapchain& chain) {
    auto vertShaderCode = readAsset(app->activity->assetManager, "shaders/contrast.vert.spv");
    auto contrastFragCode = readAsset(app->activity->assetManager, "shaders/contrast.frag.spv");
    auto brightnessFragCode = readAsset(app->activity->assetManager, "shaders/brightness.frag.spv");
    auto blurFragCode = readAsset(app->activity->assetManager, "shaders/blur.frag.spv");
    auto bloomFinalFragCode = readAsset(app->activity->assetManager, "shaders/bloom_final.frag.spv");
    auto cubeVertCode = readAsset(app->activity->assetManager, "shaders/cube.vert.spv");
    auto cubeFragCode = readAsset(app->activity->assetManager, "shaders/cube.frag.spv");

    std::vector<PostVertex> rectVerts = {
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    };
    rectVBuf = VulkanBuffer(inst);
    rectVBuf.create(rectVerts.data(), static_cast<int>(rectVerts.size() * sizeof(PostVertex)), false);

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(PostVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc[2];
    attrDesc[0].binding = 0;
    attrDesc[0].location = 0;
    attrDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[0].offset = static_cast<uint32_t>(offsetof(PostVertex, position));
    attrDesc[1].binding = 0;
    attrDesc[1].location = 1;
    attrDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[1].offset = static_cast<uint32_t>(offsetof(PostVertex, uv));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.pVertexInputState = &vertexInputInfo;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &viewportState;
    pipeInfo.pRasterizationState = &rasterizer;

    contrastPipe = new VulkanGraphicsPipeline(chain, vertShaderCode, contrastFragCode);
    contrastPipe->enableTexture();
    contrastPipe->create(pipeInfo);

    brightnessPipe = new VulkanGraphicsPipeline(chain, vertShaderCode, brightnessFragCode);
    brightnessPipe->enableTexture();
    brightnessPipe->setRenderPass(bloomRenderPass);
    brightnessPipe->create(pipeInfo);

    blurPipe = new VulkanGraphicsPipeline(chain, vertShaderCode, blurFragCode);
    blurPipe->enableTexture();
    blurPipe->setRenderPass(bloomRenderPass);
    blurPipe->create(pipeInfo);

    {
        VkDescriptorSetLayoutBinding b0{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutBinding b1{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutBinding bindings[] = {b0, b1};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, bindings};
        vkCreateDescriptorSetLayout(inst.device, &li, nullptr, &bloomFinalLayout);
    }

    bloomFinalPipe = new VulkanGraphicsPipeline(chain, vertShaderCode, bloomFinalFragCode);
    bloomFinalPipe->enableTexture();
    bloomFinalPipe->setDescriptorSetLayout(bloomFinalLayout);
    bloomFinalPipe->create(pipeInfo);

    {
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 0;
        samplerBinding.descriptorCount = 1;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.pImmutableSamplers = nullptr;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 1;
        uboBinding.descriptorCount = 1;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.pImmutableSamplers = nullptr;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding modelBinding{};
        modelBinding.binding = 2;
        modelBinding.descriptorCount = 1;
        modelBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        modelBinding.pImmutableSamplers = nullptr;
        modelBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::vector<VkDescriptorSetLayoutBinding> bindings = {samplerBinding, uboBinding, modelBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(inst.device, &layoutInfo, nullptr, &descriptorSetLayout);
    }

    scenePipe = new VulkanGraphicsPipeline(chain, cubeVertCode, cubeFragCode);
    scenePipe->enableTexture();
    scenePipe->enableUniformBuffer();
    scenePipe->setUniformBufferBindingCount(2);
    scenePipe->setDescriptorSetLayout(descriptorSetLayout);
    scenePipe->enableDepthTest();

    VkVertexInputBindingDescription sceneBinding{};
    sceneBinding.binding = 0;
    sceneBinding.stride = sizeof(float) * 8;
    sceneBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription sceneAttrs[3];
    sceneAttrs[0].location = 0;
    sceneAttrs[0].binding = 0;
    sceneAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    sceneAttrs[0].offset = 0;
    sceneAttrs[1].location = 1;
    sceneAttrs[1].binding = 0;
    sceneAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    sceneAttrs[1].offset = sizeof(float) * 3;
    sceneAttrs[2].location = 2;
    sceneAttrs[2].binding = 0;
    sceneAttrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    sceneAttrs[2].offset = sizeof(float) * 6;

    VkPipelineVertexInputStateCreateInfo sceneVertexInput{};
    sceneVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    sceneVertexInput.vertexBindingDescriptionCount = 1;
    sceneVertexInput.pVertexBindingDescriptions = &sceneBinding;
    sceneVertexInput.vertexAttributeDescriptionCount = 3;
    sceneVertexInput.pVertexAttributeDescriptions = sceneAttrs;

    VkPipelineInputAssemblyStateCreateInfo sceneInput{};
    sceneInput.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    sceneInput.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    sceneInput.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo sceneViewportState{};
    sceneViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    sceneViewportState.viewportCount = 1;
    sceneViewportState.scissorCount = 1;

    VkGraphicsPipelineCreateInfo scenePipeInfo{};
    scenePipeInfo.pVertexInputState = &sceneVertexInput;
    scenePipeInfo.pInputAssemblyState = &sceneInput;
    scenePipeInfo.pViewportState = &sceneViewportState;
    scenePipe->setRenderPass(offRenderPass);
    scenePipe->create(scenePipeInfo);

    int texW, texH;
    bool textureOK = true;
    std::vector<unsigned char> pixels;
    try {
        pixels = loadPngRgbaAsset(app->activity->assetManager, "sample.png", texW, texH);
        if (pixels.empty()) throw std::runtime_error("empty pixels");
    } catch (...) {
        textureOK = false;
        pixels = std::vector<unsigned char>(4, 255);
        texW = 1;
        texH = 1;
    }

    srcTexture = VulkanTexture(inst);
    srcTexture.load(pixels.data(), texW, texH);

    GlobalUniform3D sceneG{};
    sceneGUBO = VulkanBuffer(inst);
    sceneGUBO.create(&sceneG, sizeof(sceneG), true);

    ModelUniform sceneM{};
    sceneMUBO = VulkanBuffer(inst);
    sceneMUBO.create(&sceneM, sizeof(sceneM), true);

    scenePipe->createDescriptors(sceneGUBO, &srcTexture);
    {
        VkDescriptorBufferInfo modelInfo{};
        modelInfo.buffer = sceneMUBO.buffer;
        modelInfo.offset = 0;
        modelInfo.range = sceneMUBO.size;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = scenePipe->descriptorSets[i];
            write.dstBinding = 2;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &modelInfo;
            vkUpdateDescriptorSets(inst.device, 1, &write, 0, nullptr);
        }
    }

    MeshData cubeMesh = createBoxMesh(2.0f, 2.0f, 2.0f);
    sceneVBuf = VulkanBuffer(inst);
    sceneVBuf.create(cubeMesh.vertices.data(), static_cast<int>(cubeMesh.vertices.size() * sizeof(float)), false);
    sceneIBuf = VulkanBuffer(inst);
    sceneIBuf.createIndex(cubeMesh.indices.data(), static_cast<int>(cubeMesh.indices.size() * sizeof(uint32_t)));
    sceneIndexCount = static_cast<int>(cubeMesh.indices.size());

    offAsTexture = VulkanTexture(inst);
    offAsTexture.imageView = offView;
    offAsTexture.sampler = offSampler;
    contrastPipe->createTextureDescriptor(offAsTexture);
    brightnessPipe->createTextureDescriptor(offAsTexture);

    bloom1AsTex = VulkanTexture(inst);
    bloom1AsTex.imageView = bloomView1;
    bloom1AsTex.sampler = offSampler;
    bloom2AsTex = VulkanTexture(inst);
    bloom2AsTex.imageView = bloomView2;
    bloom2AsTex.sampler = offSampler;

    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = blurPipe->descriptorSetLayout;
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, bloomPool, MAX_FRAMES_IN_FLIGHT, layouts};
        vkAllocateDescriptorSets(inst.device, &ai, blurDS1);
        vkAllocateDescriptorSets(inst.device, &ai, blurDS2);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo info1{offSampler, bloomView1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo info2{offSampler, bloomView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w1{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, blurDS1[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &info1, nullptr, nullptr};
            VkWriteDescriptorSet w2{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, blurDS2[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &info2, nullptr, nullptr};
            vkUpdateDescriptorSets(inst.device, 1, &w1, 0, nullptr);
            vkUpdateDescriptorSets(inst.device, 1, &w2, 0, nullptr);
        }
    }

    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = bloomFinalLayout;
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, bloomPool, MAX_FRAMES_IN_FLIGHT, layouts};
        vkAllocateDescriptorSets(inst.device, &allocInfo, bloomFinalDS);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo sceneInfo{offSampler, offView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo bloomInfo{offSampler, bloomView1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[2];
            w[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, bloomFinalDS[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, nullptr, nullptr};
            w[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, bloomFinalDS[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bloomInfo, nullptr, nullptr};
            vkUpdateDescriptorSets(inst.device, 2, w, 0, nullptr);
        }
    }
}

VkCommandBuffer PostProcessExample::beginTempCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = tempCommandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(savedDevice, &allocInfo, &cmd);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void PostProcessExample::endTempCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(savedGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(savedGraphicsQueue);
    vkFreeCommandBuffers(savedDevice, tempCommandPool, 1, &cmd);
}

void PostProcessExample::init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) {
    this->app = app;
    screenWidth = width;
    screenHeight = height;
    useBloom = false;
    startTime = std::chrono::steady_clock::now();

    savedDevice = inst.device;
    savedPhysicalDevice = inst.physicalDevice;
    savedGraphicsQueue = inst.graphicsQueue;

    QueueFamilyIndices indices = findQueueFamilies(inst.physicalDevice, inst.surface);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily.first;
    vkCreateCommandPool(inst.device, &poolInfo, nullptr, &tempCommandPool);

    createOffscreenResources(inst, chain);
    createBloomResources(inst, chain);
    createSceneResources(inst, chain);

    initialized = true;
}

void PostProcessExample::cleanup(VulkanInstance& inst) {
    if (!initialized) return;
    vkDeviceWaitIdle(inst.device);
    initialized = false;

    if (scenePipe) {
        scenePipe->destroy();
        delete scenePipe; scenePipe = nullptr;
    }
    if (contrastPipe) {
        contrastPipe->destroy();
        delete contrastPipe; contrastPipe = nullptr;
    }
    if (brightnessPipe) {
        brightnessPipe->destroy();
        delete brightnessPipe; brightnessPipe = nullptr;
    }
    if (blurPipe) {
        blurPipe->destroy();
        delete blurPipe; blurPipe = nullptr;
    }
    if (bloomFinalPipe) {
        bloomFinalPipe->destroy();
        delete bloomFinalPipe; bloomFinalPipe = nullptr;
    }

    srcTexture.destroy();

    sceneGUBO.destroy();
    sceneMUBO.destroy();
    sceneVBuf.destroy();
    sceneIBuf.destroy();
    rectVBuf.destroy();

    vkDestroyDescriptorSetLayout(inst.device, bloomFinalLayout, nullptr);
    vkDestroyDescriptorSetLayout(inst.device, descriptorSetLayout, nullptr);

    if (tempCommandPool) {
        vkDestroyCommandPool(inst.device, tempCommandPool, nullptr);
        tempCommandPool = VK_NULL_HANDLE;
    }

    destroyOffscreenResources(inst.device);
    destroyBloomResources(inst.device);
}

void PostProcessExample::destroyOffscreenResources(VkDevice device) {
    if (offSampler) vkDestroySampler(device, offSampler, nullptr);
    if (offView) vkDestroyImageView(device, offView, nullptr);
    if (offDepthView) vkDestroyImageView(device, offDepthView, nullptr);
    if (offFramebuffer) vkDestroyFramebuffer(device, offFramebuffer, nullptr);
    if (offRenderPass) vkDestroyRenderPass(device, offRenderPass, nullptr);
    if (offImage) vkDestroyImage(device, offImage, nullptr);
    if (offMemory) vkFreeMemory(device, offMemory, nullptr);
    if (offDepthImage) vkDestroyImage(device, offDepthImage, nullptr);
    if (offDepthMemory) vkFreeMemory(device, offDepthMemory, nullptr);
    offSampler = VK_NULL_HANDLE;
    offView = VK_NULL_HANDLE;
    offDepthView = VK_NULL_HANDLE;
    offFramebuffer = VK_NULL_HANDLE;
    offRenderPass = VK_NULL_HANDLE;
    offImage = VK_NULL_HANDLE;
    offMemory = VK_NULL_HANDLE;
    offDepthImage = VK_NULL_HANDLE;
    offDepthMemory = VK_NULL_HANDLE;
}

void PostProcessExample::destroyBloomResources(VkDevice device) {
    if (bloomPool) {
        vkDestroyDescriptorPool(device, bloomPool, nullptr);
        bloomPool = VK_NULL_HANDLE;
    }
    if (bloomFB1) { vkDestroyFramebuffer(device, bloomFB1, nullptr); bloomFB1 = VK_NULL_HANDLE; }
    if (bloomFB2) { vkDestroyFramebuffer(device, bloomFB2, nullptr); bloomFB2 = VK_NULL_HANDLE; }
    if (bloomRenderPass) { vkDestroyRenderPass(device, bloomRenderPass, nullptr); bloomRenderPass = VK_NULL_HANDLE; }
    if (bloomView1) { vkDestroyImageView(device, bloomView1, nullptr); bloomView1 = VK_NULL_HANDLE; }
    if (bloomView2) { vkDestroyImageView(device, bloomView2, nullptr); bloomView2 = VK_NULL_HANDLE; }
    if (bloomImage1) { vkDestroyImage(device, bloomImage1, nullptr); bloomImage1 = VK_NULL_HANDLE; }
    if (bloomImage2) { vkDestroyImage(device, bloomImage2, nullptr); bloomImage2 = VK_NULL_HANDLE; }
    if (bloomMemory1) { vkFreeMemory(device, bloomMemory1, nullptr); bloomMemory1 = VK_NULL_HANDLE; }
    if (bloomMemory2) { vkFreeMemory(device, bloomMemory2, nullptr); bloomMemory2 = VK_NULL_HANDLE; }
}

void PostProcessExample::draw(VulkanRenderer& renderer, uint32_t currentFrame) {
    if (!initialized) return;
    VulkanSwapchain& chain = renderer.swapChain;

    VkCommandBuffer cmdOff = beginTempCommands();

    VkClearValue offClears[2];
    offClears[0].color = {{0.3f, 0.3f, 0.3f, 1.0f}};
    offClears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo offBegin{};
    offBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    offBegin.renderPass = offRenderPass;
    offBegin.framebuffer = offFramebuffer;
    offBegin.renderArea.offset = {0, 0};
    offBegin.renderArea.extent = chain.swapChainExtent;
    offBegin.clearValueCount = 2;
    offBegin.pClearValues = offClears;
    vkCmdBeginRenderPass(cmdOff, &offBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipe->graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)chain.swapChainExtent.width;
    viewport.height = (float)chain.swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdOff, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = chain.swapChainExtent;
    vkCmdSetScissor(cmdOff, 0, 1, &scissor);

    VkBuffer vb[] = {sceneVBuf.buffer};
    VkDeviceSize voff[] = {0};
    vkCmdBindVertexBuffers(cmdOff, 0, 1, vb, voff);
    vkCmdBindIndexBuffer(cmdOff, sceneIBuf.buffer, 0, VK_INDEX_TYPE_UINT32);

    {
        float aspect = (float)chain.swapChainExtent.width / (float)chain.swapChainExtent.height;
        Mat4 proj = Mat4::perspective(45.0f * (float)M_PI / 180.0f, aspect, 0.1f, 100.0f);
        Vec3 eye(6.0f, 0.0f, 8.0f);
        Vec3 center(0.0f, 0.0f, 0.0f);
        Vec3 up(0.0f, 1.0f, 0.0f);
        Mat4 view = Mat4::lookAt(eye, center, up);
        Mat4 pv = proj * view;

        GlobalUniform3D sceneG;
        std::memcpy(sceneG.ProjView, pv.data(), sizeof(sceneG.ProjView));
        sceneG.camera[0] = eye.x; sceneG.camera[1] = eye.y; sceneG.camera[2] = eye.z; sceneG.camera[3] = 0.0f;
        sceneG.lights[0] = {{4,4,4,0}, {1,1,1,0}};
        sceneG.lights[1] = {{-4,4,4,0}, {1,0,0,0}};
        sceneG.lights[2] = {{0,4,-4,0}, {0,0,1,0}};
        sceneGUBO.update(&sceneG, sizeof(sceneG));

        float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        Mat4 model = Mat4::rotate(Mat4::identity(), elapsed, Vec3(0.0f, 1.0f, 0.0f));
        ModelUniform sceneM;
        std::memcpy(sceneM.model, model.data(), sizeof(sceneM.model));
        sceneMUBO.update(&sceneM, sizeof(sceneM));
    }

    VkDescriptorSet offSet = scenePipe->descriptorSets[0];
    vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipe->pipelineLayout, 0, 1, &offSet, 0, nullptr);
    vkCmdDrawIndexed(cmdOff, sceneIndexCount, 1, 0, 0, 0);
    vkCmdEndRenderPass(cmdOff);

    if (useBloom) {
        VkBuffer vbufs[] = {rectVBuf.buffer};
        VkDeviceSize voffs[] = {0};
        vkCmdBindVertexBuffers(cmdOff, 0, 1, vbufs, voffs);

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, bloomRenderPass, bloomFB1, {{0,0}, chain.swapChainExtent}, 1, &clearColor};

        vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, brightnessPipe->graphicsPipeline);
        vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, brightnessPipe->pipelineLayout, 0, 1, &brightnessPipe->descriptorSets[0], 0, nullptr);
        vkCmdDraw(cmdOff, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmdOff);

        rpBegin.framebuffer = bloomFB2;
        vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe->graphicsPipeline);
        float horizontalDir[2] = {1.0f, 0.0f};
        vkCmdPushConstants(cmdOff, blurPipe->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, horizontalDir);
        vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe->pipelineLayout, 0, 1, &blurDS1[0], 0, nullptr);
        vkCmdDraw(cmdOff, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmdOff);

        rpBegin.framebuffer = bloomFB1;
        vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe->graphicsPipeline);
        float verticalDir[2] = {0.0f, 1.0f};
        vkCmdPushConstants(cmdOff, blurPipe->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, verticalDir);
        vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe->pipelineLayout, 0, 1, &blurDS2[0], 0, nullptr);
        vkCmdDraw(cmdOff, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmdOff);
    }

    endTempCommands(cmdOff);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VulkanGraphicsPipeline& currentPipe = useBloom ? *bloomFinalPipe : *contrastPipe;
    VkCommandBuffer cmd = renderer.begin(currentPipe, &clearColor, 1);

    VkViewport viewport2{};
    viewport2.x = 0.0f;
    viewport2.y = 0.0f;
    viewport2.width = (float)chain.swapChainExtent.width;
    viewport2.height = (float)chain.swapChainExtent.height;
    viewport2.minDepth = 0.0f;
    viewport2.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport2);

    VkRect2D scissor2{};
    scissor2.offset = {0, 0};
    scissor2.extent = chain.swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor2);

    VkBuffer buffers2[] = {rectVBuf.buffer};
    VkDeviceSize offsets2[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers2, offsets2);

    if (useBloom) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomFinalPipe->pipelineLayout, 0, 1, &bloomFinalDS[renderer.currentFrame], 0, nullptr);
    } else {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, contrastPipe->pipelineLayout, 0, 1, &contrastPipe->descriptorSets[renderer.currentFrame], 0, nullptr);
    }
    vkCmdDraw(cmd, 6, 1, 0, 0);

    renderer.end();
}

void PostProcessExample::onTouch(float x, float y, int action, bool down) {
    if (action == 1) {
        useBloom = !useBloom;
        LOGI("Post-processing toggled: %s", useBloom ? "Bloom" : "Contrast");
    }
}
