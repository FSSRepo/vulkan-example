#include "example_menu.h"
#include "example_cube.h"
#include "example_postprocess.h"
#include "text_renderer.h"
#include <android/log.h>
#include <vector>
#include <array>
#include <cstring>
#include <cstdio>
#include <algorithm>

#define TAG "Vulkan-Menu"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))

static std::vector<ExampleEntry>& registry() {
    static std::vector<ExampleEntry> reg;
    return reg;
}

void registerExample(const std::string& name, IExample* (*create)()) {
    registry().push_back({name, create});
}

IExample* createCubeExample() {
    return new CubeExample();
}

IExample* createMenuExample() {
    return new MenuExample();
}

namespace {
    struct Registrar {
        Registrar() {
            registerExample("Cube", createCubeExample);
            registerExample("PostProcess", createPostProcessExample);
        }
    } registrar;
}

void MenuExample::addExample(const ExampleEntry& entry) {
    examples.push_back(entry);
}

void MenuExample::setupTextPipeline(VulkanInstance& inst, VulkanSwapchain& chain) {
    auto vertShaderCode = readAsset(app->activity->assetManager, "shaders/text.vert.spv");
    auto fragShaderCode = readAsset(app->activity->assetManager, "shaders/text.frag.spv");

    textPipe = new VulkanGraphicsPipeline(chain, vertShaderCode, fragShaderCode);
    textPipe->enableTexture();
    textPipe->disableDepthTest();

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(TextVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[3];
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(TextVertex, pos);
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(TextVertex, uv);
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(TextVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

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

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pColorBlendState = &colorBlending;
    textPipe->create(pipelineInfo);
}

void MenuExample::init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) {
    this->app = app;
    screenWidth = width;
    screenHeight = height;
    examples = registry();
    if (examples.empty()) {
        addExample({"Cube", createCubeExample});
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(inst.physicalDevice, &props);
    uint32_t vMajor = VK_API_VERSION_MAJOR(props.apiVersion);
    uint32_t vMinor = VK_API_VERSION_MINOR(props.apiVersion);
    snprintf(gpuName, sizeof(gpuName), "%s  Vulkan %d.%d", props.deviceName, vMajor, vMinor);
    snprintf(hintMessage, sizeof(hintMessage), "Select an example");

    auto vertShaderCode = readAsset(app->activity->assetManager, "shaders/cube.vert.spv");
    auto fragShaderCode = readAsset(app->activity->assetManager, "shaders/cube.frag.spv");

    {
        gpipe = new VulkanGraphicsPipeline(chain, vertShaderCode, fragShaderCode);
        gpipe->disableDepthTest();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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

    font = new Font(inst);
    font->loadMetrics(app->activity->assetManager, "windows.fft");
    font->loadTexture(app->activity->assetManager, "font_texture.png");

    setupTextPipeline(inst, chain);
    textPipe->createTextureDescriptor(font->texture);

    gpuNameText = new TextView(inst, font);
    gpuNameText->setText(gpuName);
    gpuNameText->setTextSize(0.045f);
    gpuNameText->setAlignment(TextView::ALIGN_CENTER);
    gpuNameText->setColor(0.6f, 0.8f, 1.0f);
    gpuNameText->setPosition(0.0f, 0.92f);
    gpuNameText->updateText((float)width, (float)height);

    hintText = new TextView(inst, font);
    hintText->setText(hintMessage);
    hintText->setTextSize(0.035f);
    hintText->setAlignment(TextView::ALIGN_CENTER);
    hintText->setColor(0.6f, 0.6f, 0.7f);
    hintText->setPosition(0.0f, -0.88f);
    hintText->updateText((float)width, (float)height);

    int itemCount = (int)examples.size();
    float itemH = 1.0f / (itemCount + 1);
    float startY = 1.0f - itemH * 0.5f - 0.07f;

    for (int i = 0; i < itemCount; i++) {
        TextView* tv = new TextView(inst, font);
        tv->setText(examples[i].name);
        tv->setTextSize(0.08f);
        tv->setAlignment(TextView::ALIGN_CENTER);
        tv->setColor(0.9f, 0.9f, 1.0f);
        tv->setPosition(0.0f, startY - i * itemH);
        tv->updateText((float)width, (float)height);
        textViews.push_back(tv);
    }
}

void MenuExample::updateHint(int index) {
    if (index >= 0 && index < (int)examples.size()) {
        const std::string& name = examples[index].name;
        if (name == "Cube") {
            snprintf(hintMessage, sizeof(hintMessage), "Rotating cube with 3 dynamic lights");
        } else if (name == "Texture") {
            snprintf(hintMessage, sizeof(hintMessage), "Textured cube example");
        } else if (name == "Depth") {
            snprintf(hintMessage, sizeof(hintMessage), "Depth buffer visualization");
        } else if (name == "PostProcess") {
            snprintf(hintMessage, sizeof(hintMessage), "Bloom post-processing effect");
        } else {
            snprintf(hintMessage, sizeof(hintMessage), "Touch to launch");
        }
    } else {
        snprintf(hintMessage, sizeof(hintMessage), "Select an example");
    }
    if (hintText) {
        hintText->setText(hintMessage);
        hintText->updateText((float)screenWidth, (float)screenHeight);
    }
}

void MenuExample::cleanup(VulkanInstance& inst) {
    vkDeviceWaitIdle(inst.device);
    if (hintText) {
        hintText->destroy();
        delete hintText;
        hintText = nullptr;
    }
    if (gpuNameText) {
        gpuNameText->destroy();
        delete gpuNameText;
        gpuNameText = nullptr;
    }
    for (size_t i = 0; i < textViews.size(); i++) {
        textViews[i]->destroy();
        delete textViews[i];
    }
    textViews.clear();
    if (font) {
        font->texture.destroy();
        delete font;
        font = nullptr;
    }
    if (textPipe) {
        textPipe->destroy();
        delete textPipe;
        textPipe = nullptr;
    }
    if (gpipe) {
        gpipe->destroy();
        delete gpipe;
        gpipe = nullptr;
    }
}

void MenuExample::draw(VulkanRenderer& renderer, uint32_t currentFrame) {
    VulkanSwapchain& chain = renderer.swapChain;

    VkClearValue clear = {{{0.12f, 0.12f, 0.18f, 1.0f}}};
    VkCommandBuffer cmd = renderer.begin(*gpipe, &clear, 1);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)chain.swapChainExtent.width;
    viewport.height = (float)chain.swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = chain.swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipe->graphicsPipeline);

    float aspect = (float)screenWidth / (float)screenHeight;
    uint32_t vw = chain.swapChainExtent.width;
    uint32_t vh = chain.swapChainExtent.height;

    if (!textPipe->descriptorSets.empty()) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            textPipe->pipelineLayout, 0, 1,
            &textPipe->descriptorSets[renderer.currentFrame], 0, nullptr);
    }

    if (gpuNameText) {
        gpuNameText->render(cmd, *textPipe, aspect, vw, vh);
    }

    for (int i = 0; i < (int)textViews.size(); i++) {
        textViews[i]->render(cmd, *textPipe, aspect, vw, vh);
    }

    if (hintText) {
        hintText->render(cmd, *textPipe, aspect, vw, vh);
    }

    renderer.end();
}

int MenuExample::getTouchedItem(float touchX, float touchY, int width, int height) {
    int itemCount = (int)examples.size();
    float itemH_ndc = 1.0f / (itemCount + 1);
    float startY_ndc = 1.0f - itemH_ndc * 0.5f - 0.07f;
    float halfW_ndc = 0.4f;

    float ndcX = 2.0f * touchX / width - 1.0f;
    float ndcY = 1.0f - 2.0f * touchY / height;

    for (int i = 0; i < itemCount; i++) {
        float itemY_ndc = startY_ndc - i * itemH_ndc;
        if (ndcX >= -halfW_ndc && ndcX <= halfW_ndc &&
            ndcY >= itemY_ndc - itemH_ndc * 0.35f && ndcY <= itemY_ndc + itemH_ndc * 0.35f) {
            return i;
        }
    }
    return -1;
}

void MenuExample::onTouch(float x, float y, int action, bool down) {
    if (action == 0) {
        lastTouchX = x;
        lastTouchY = y;
        touched = true;
        int newSelected = getTouchedItem(x, y, screenWidth, screenHeight);
        if (newSelected != selectedItem) {
            if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
                textViews[selectedItem]->setColor(0.9f, 0.9f, 1.0f);
            }
            selectedItem = newSelected;
            if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
                textViews[selectedItem]->setColor(1.0f, 0.8f, 0.2f);
            }
            updateHint(selectedItem);
        }
    } else if (action == 1 && touched) {
        touched = false;
        if (!g_currentEngine) return;
        if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
            textViews[selectedItem]->setColor(0.9f, 0.9f, 1.0f);
        }
        int item = selectedItem;
        selectedItem = -1;
        updateHint(-1);
        if (item >= 0 && item < (int)examples.size()) {
            if (examples[item].create) {
                IExample* example = examples[item].create();
                if (example) {
                    androidSetExample(g_currentEngine, example);
                }
            }
        }
    } else if (action == 2 && touched) {
        int newSelected = getTouchedItem(x, y, screenWidth, screenHeight);
        if (newSelected != selectedItem) {
            if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
                textViews[selectedItem]->setColor(0.9f, 0.9f, 1.0f);
            }
            selectedItem = newSelected;
            if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
                textViews[selectedItem]->setColor(1.0f, 0.8f, 0.2f);
            }
            updateHint(selectedItem);
        }
    } else if (action == 3) {
        if (selectedItem >= 0 && selectedItem < (int)textViews.size()) {
            textViews[selectedItem]->setColor(0.9f, 0.9f, 1.0f);
        }
        touched = false;
        selectedItem = -1;
        updateHint(-1);
    }
}
