#include "example_cube.h"
#include <android/log.h>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstring>

#define TAG "Vulkan-Example"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

void CubeExample::createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 1;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding modelLayoutBinding{};
    modelLayoutBinding.binding = 2;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    modelLayoutBinding.pImmutableSamplers = nullptr;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {samplerLayoutBinding, uboLayoutBinding, modelLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
    gpipe->setDescriptorSetLayout(descriptorSetLayout);
}

void CubeExample::createDescriptorPool(VkDevice device) {
    const uint32_t maxSets = MAX_FRAMES_IN_FLIGHT * 16;
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxSets;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = maxSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void CubeExample::setupVertexInput() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 8;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[1].offset = sizeof(float)*3;
    attrs[2].location = 2; attrs[2].binding = 0; attrs[2].format = VK_FORMAT_R32G32_SFLOAT;  attrs[2].offset = sizeof(float)*6;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    gpipe->create(pipelineInfo);
}

void CubeExample::init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) {
    auto vertShaderCode = readAsset(app->activity->assetManager, "shaders/cube.vert.spv");
    auto fragShaderCode = readAsset(app->activity->assetManager, "shaders/cube.frag.spv");

    gpipe = new VulkanGraphicsPipeline(chain, vertShaderCode, fragShaderCode);
    gpipe->enableTexture();
    gpipe->enableUniformBuffer();

    createDescriptorSetLayout(inst.device);
    setupVertexInput();

    int texW = 1, texH = 1;
    std::vector<unsigned char> pixels;
    textureOK = true;
    try {
        pixels = loadPngRgbaAsset(app->activity->assetManager, "sample.png", texW, texH);
        if (pixels.empty()) throw std::runtime_error("empty pixels");
    } catch (...) {
        textureOK = false;
        pixels = std::vector<unsigned char>(4, 255);
    }

    smodel.init(inst);
    smodel.camera.aspect = (float)width / (float)height;
    smodel.addLight(Vec4(4.0f, 4.0f, 4.0f, 0.0f), Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    smodel.addLight(Vec4(-4.0f, -4.0f, -4.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 0.0f));
    smodel.addLight(Vec4(0.0f, 0.0f, -4.0f, 0.0f), Vec4(0.0f, 0.0f, 1.0f, 0.0f));

    createDescriptorPool(inst.device);

    if (textureOK) {
        tmpTex = VulkanTexture(inst);
        tmpTex.load(pixels.data(), texW, texH);
    }

    auto lightMesh = createBoxMesh(0.5f, 0.5f, 0.5f);
    light0.init(inst, lightMesh.vertices, lightMesh.indices);
    light1.init(inst, lightMesh.vertices, lightMesh.indices);
    light2.init(inst, lightMesh.vertices, lightMesh.indices);
    light0.material.color = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    light1.material.color = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    light2.material.color = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    light0.material.createTextureFromColor(inst);
    light1.material.createTextureFromColor(inst);
    light2.material.createTextureFromColor(inst);
    light0.setPosition(Vec3(4.0f, 4.0f, 4.0f));
    light1.setPosition(Vec3(-4.0f, -4.0f, -4.0f));
    light2.setPosition(Vec3(0.0f, 0.0f, -4.0f));

    auto cubeMesh = createBoxMesh(2.0f, 2.0f, 2.0f);
    model.init(inst, cubeMesh.vertices, cubeMesh.indices);
    if (textureOK) {
        model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        model.material.texture = tmpTex;
    } else {
        model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        model.material.createTextureFromColor(inst);
    }

    VkDevice device = inst.device;
    light0.setupDescriptors(device, descriptorPool, descriptorSetLayout, smodel);
    light1.setupDescriptors(device, descriptorPool, descriptorSetLayout, smodel);
    light2.setupDescriptors(device, descriptorPool, descriptorSetLayout, smodel);
    model.setupDescriptors(device, descriptorPool, descriptorSetLayout, smodel);
}

void CubeExample::cleanup(VulkanInstance& inst) {
    vkDeviceWaitIdle(inst.device);
    model.destroy();
    light0.destroy();
    light1.destroy();
    light2.destroy();
    smodel.destroy();
    if (textureOK) tmpTex.destroy();
    vkDestroyDescriptorPool(inst.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(inst.device, descriptorSetLayout, nullptr);
    if (gpipe) {
        gpipe->destroy();
        delete gpipe;
        gpipe = nullptr;
    }
}

void CubeExample::draw(VulkanRenderer& renderer, uint32_t currentFrame) {
    VulkanSwapchain& chain = renderer.swapChain;

    VkClearValue clears[2];
    clears[0] = {{{0.3f, 0.3f, 0.3f, 1.0f}}};
    clears[1].depthStencil = {1.0f, 0};
    VkCommandBuffer cmd = renderer.begin(*gpipe, clears, 2);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) chain.swapChainExtent.width;
    viewport.height = (float) chain.swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = chain.swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    smodel.updateGlobal(currentFrame);
    model.render(cmd, smodel, *gpipe, currentFrame);
    light0.render(cmd, smodel, *gpipe, currentFrame);
    light1.render(cmd, smodel, *gpipe, currentFrame);
    light2.render(cmd, smodel, *gpipe, currentFrame);
    renderer.end();
}

void CubeExample::onTouch(float x, float y, int action, bool down) {
    if (action == 0) {
        lastTouchX = x;
        lastTouchY = y;
    } else if (action == 2) {
        float dx = x - lastTouchX;
        float dy = y - lastTouchY;
        lastTouchX = x;
        lastTouchY = y;
        smodel.camera.orbit(dx * 0.005f, dy * 0.005f);
    }
}
