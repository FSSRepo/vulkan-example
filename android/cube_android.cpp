#include "vulkan_wrapper.h"
#include "vkApp.h"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <array>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics_math.h"

#define TAG "Vulkan-Example"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

static std::vector<char> readAsset(AAssetManager* mgr, const std::string& filename) {
    AAsset* asset = AAssetManager_open(mgr, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("failed to open asset: %s", filename.c_str());
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

static std::vector<unsigned char> load_png_rgba_asset(AAssetManager* mgr, const char* filename, int &outW, int &outH) {
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("failed to open asset: %s", filename);
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<unsigned char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    int n;
    unsigned char* data = stbi_load_from_memory(buffer.data(), (int)size, &outW, &outH, &n, STBI_rgb_alpha);
    if (!data) {
        LOGE("failed to load image with stb_image: %s", filename);
        return {};
    }
    std::vector<unsigned char> pixels((size_t)outW * (size_t)outH * 4);
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);
    return pixels;
}

struct LightData { float position[4]; float color[4]; };
struct GlobalUniform { float ProjView[16]; float camera[4]; LightData lights[3]; };
struct ModelUniform { float model[16]; };

class Camera {
public:
    Vec3 position;
    Vec3 up;
    Vec3 direction;
    float fov;
    float aspect;
    float nearClip;
    float farClip;
    float yaw;
    float pitch;
    Camera() {
        position = Vec3(20.0f, -10.0f, 20.0f);
        up = Vec3(0.0f, 1.0f, 0.0f);
        direction = -position;
        fov = 45.f * M_PI / 180.0f;
        aspect = 1.0f;
        nearClip = 0.1f;
        farClip = 100.0f;
        yaw = std::atan2(position.z, position.x);
        pitch = std::asin(position.y / position.length());
    }
    void orbit(float nx, float ny) {
        float sens = 1.0f;
        yaw += nx * sens;
        pitch += ny * sens;
        float limit = 89.0f * M_PI / 180.0f;
        if (pitch > limit) pitch = limit;
        if (pitch < -limit) pitch = -limit;
        float radius = position.length();
        float cx = std::cos(pitch);
        position.x = radius * cx * std::cos(yaw);
        position.y = radius * std::sin(pitch);
        position.z = radius * cx * std::sin(yaw);
        direction = -position;
    }
    void move(float nx, float ny) {
        Vec3 dirN = direction.normalize();
        Vec3 right = dirN.cross(up).normalize();
        position = position + right * nx + (-dirN) * ny;
        direction = -position;
    }
    Mat4 getProjViewMatrix() const {
        Mat4 proj = Mat4::perspective(fov, aspect, nearClip, farClip);
        Mat4 view = Mat4::lookAt(position, position + direction, up);
        return proj * view;
    }
};

class ShaderModel {
public:
    Camera camera;
    GlobalUniform gdata{};
    VulkanBuffer globalBuffer[MAX_FRAMES_IN_FLIGHT];
    int lightCount = 0;
    void init(VulkanInstance &inst) {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            globalBuffer[i] = VulkanBuffer(inst);
            globalBuffer[i].create(&gdata, sizeof(gdata), true);
        }
    }
    void addLight(const Vec4 &pos, const Vec4 &col) {
        if (lightCount >= 3) return;
        gdata.lights[lightCount].position[0] = pos.x;
        gdata.lights[lightCount].position[1] = pos.y;
        gdata.lights[lightCount].position[2] = pos.z;
        gdata.lights[lightCount].position[3] = pos.w;
        gdata.lights[lightCount].color[0] = col.x;
        gdata.lights[lightCount].color[1] = col.y;
        gdata.lights[lightCount].color[2] = col.z;
        gdata.lights[lightCount].color[3] = col.w;
        lightCount++;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            globalBuffer[i].update(&gdata, sizeof(gdata));
        }
    }
    void updateGlobal(uint32_t currentFrame) {
        Mat4 pv = camera.getProjViewMatrix();
        std::memcpy(gdata.ProjView, pv.data(), sizeof(gdata.ProjView));
        gdata.camera[0] = camera.position.x;
        gdata.camera[1] = camera.position.y;
        gdata.camera[2] = camera.position.z;
        gdata.camera[3] = 0.0f;
        globalBuffer[currentFrame].update(&gdata, sizeof(gdata));
    }
    void destroy() {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            globalBuffer[i].destroy();
        }
    }
};

struct MeshData { std::vector<float> vertices; std::vector<uint32_t> indices; };

static MeshData createBoxMesh(float w, float h, float d) {
    MeshData mesh;
    mesh.vertices = {
        -w,-h, d,  0,0, 1,  0,0,   w,-h, d,  0,0, 1,  1,0,   w, h, d,  0,0, 1,  1,1,  -w, h, d,  0,0, 1,  0,1,
         w,-h, d,  1,0,0,  0,0,   w,-h,-d,  1,0,0,  1,0,   w, h,-d,  1,0,0,  1,1,   w, h, d,  1,0,0,  0,1,
         w, h,-d,  0,0,-1, 1,1,  -w, h,-d,  0,0,-1, 0,1,  -w,-h,-d, 0,0,-1, 0,0,   w,-h,-d, 0,0,-1, 1,0,
        -w,-h,-d, -1,0,0, 0,0,  -w,-h, d, -1,0,0, 1,0,  -w, h, d, -1,0,0, 1,1,  -w, h,-d, -1,0,0, 0,1,
        -w, h,-d, 0,1,0, 0,1,   w, h,-d, 0,1,0, 1,1,   w, h, d, 0,1,0, 1,0,  -w, h, d, 0,1,0, 0,0,
        -w,-h, d, 0,-1,0, 0,0,   w,-h, d, 0,-1,0, 1,0,   w,-h,-d, 0,-1,0, 1,1,  -w,-h,-d, 0,-1,0, 0,1,
    };
    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9,10, 8,10,11,
        12,13,14,12,14,15,
        16,17,18,16,18,19,
        20,21,22,20,22,23
    };
    return mesh;
}

static VkDescriptorPool gDescriptorPool{};
static VkDescriptorSetLayout gDescriptorSetLayout{};

struct Material {
    Vec4 color;
    VulkanTexture texture;
    std::vector<VkDescriptorSet> descriptorSets;
    bool descriptorsReady = false;
    void createTextureFromColor(VulkanInstance &inst) {
        texture = VulkanTexture(inst);
        unsigned char px[4];
        auto toByte = [](float c) { float v = std::max(0.0f, std::min(1.0f, c)); return static_cast<unsigned char>(v * 255.0f); };
        px[0] = toByte(color.x);
        px[1] = toByte(color.y);
        px[2] = toByte(color.z);
        px[3] = toByte(color.w);
        texture.load(px, 1, 1);
    }
    void destroy() {
        texture.destroy();
    }
};

class ModelObject {
public:
    VulkanBuffer buffer;
    VulkanBuffer indexBuffer;
    VulkanBuffer modelUbo[MAX_FRAMES_IN_FLIGHT];
    ModelUniform mdata{};
    Mat4 modelMatrix = Mat4::identity();
    Material material;
    int vertexCount = 0;
    int indexCount = 0;
    Vec3 _pos{0.0f, 0.0f, 0.0f};
    Vec3 _rot{0.0f, 0.0f, 0.0f};
    void init(VulkanInstance &inst, const std::vector<float> &vertex_data, const std::vector<uint32_t> &index_data) {
        buffer = VulkanBuffer(inst);
        buffer.create(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)), false);
        vertexCount = static_cast<int>(buffer.size / (sizeof(float) * 8));
        indexBuffer = VulkanBuffer(inst);
        indexBuffer.createIndex(index_data.data(), static_cast<int>(index_data.size() * sizeof(uint32_t)));
        indexCount = static_cast<int>(index_data.size());
        updateMatrix();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            modelUbo[i] = VulkanBuffer(inst);
            modelUbo[i].create(&mdata, sizeof(mdata), true);
        }
    }
    void setPosition(const Vec3 &p) { _pos = p; updateMatrix(); }
    void setRotation(const Vec3 &r) { _rot = r; updateMatrix(); }
    void render(VkCommandBuffer cmd, VkDevice device, ShaderModel &smodel, VulkanGraphicsPipeline &gpipe, uint32_t currentFrame) {
        std::memcpy(mdata.model, modelMatrix.data(), sizeof(mdata.model));
        modelUbo[currentFrame].update(&mdata, sizeof(mdata));
        if (!material.descriptorsReady) {
            material.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = gDescriptorPool;
            std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, gDescriptorSetLayout);
            allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
            allocInfo.pSetLayouts = layouts.data();
            if (vkAllocateDescriptorSets(device, &allocInfo, material.descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate material descriptor sets");
            }
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = material.texture.imageView;
            imageInfo.sampler = material.texture.sampler;
            for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
                VkDescriptorBufferInfo globalInfo{};
                globalInfo.buffer = smodel.globalBuffer[f].buffer;
                globalInfo.offset = 0;
                globalInfo.range = smodel.globalBuffer[f].size;
                VkDescriptorBufferInfo modelInfo{};
                modelInfo.buffer = modelUbo[f].buffer;
                modelInfo.offset = 0;
                modelInfo.range = modelUbo[f].size;
                std::vector<VkWriteDescriptorSet> writes(3);
                writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[0].dstSet = material.descriptorSets[f];
                writes[0].dstBinding = 1;
                writes[0].dstArrayElement = 0;
                writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[0].descriptorCount = 1;
                writes[0].pBufferInfo = &globalInfo;
                writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[1].dstSet = material.descriptorSets[f];
                writes[1].dstBinding = 0;
                writes[1].dstArrayElement = 0;
                writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[1].descriptorCount = 1;
                writes[1].pImageInfo = &imageInfo;
                writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[2].dstSet = material.descriptorSets[f];
                writes[2].dstBinding = 2;
                writes[2].dstArrayElement = 0;
                writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[2].descriptorCount = 1;
                writes[2].pBufferInfo = &modelInfo;
                vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            }
            material.descriptorsReady = true;
        }
        VkDeviceSize offsets[] = {0};
        VkBuffer vb = buffer.buffer;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpipe.pipelineLayout, 0, 1, &material.descriptorSets[currentFrame], 0, nullptr);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }
    void destroy() { buffer.destroy(); indexBuffer.destroy(); material.destroy(); for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) { modelUbo[i].destroy(); } }
private:
    void updateMatrix() {
        Mat4 T = Mat4::translate(Mat4::identity(), _pos);
        Mat4 Rx = Mat4::rotate(Mat4::identity(), _rot.x, Vec3(1,0,0));
        Mat4 Ry = Mat4::rotate(Mat4::identity(), _rot.y, Vec3(0,1,0));
        Mat4 Rz = Mat4::rotate(Mat4::identity(), _rot.z, Vec3(0,0,1));
        modelMatrix = T * (Rz * Ry * Rx);
    }
};

struct Engine {
    struct android_app* app;
    VulkanInstance inst;
    VulkanSwapchain* chain = nullptr;
    VulkanGraphicsPipeline* gpipe = nullptr;
    VulkanRenderer* renderer = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    ShaderModel smodel;
    ModelObject light0, light1, light2;
    ModelObject model;
    VulkanTexture tmpTex;
    bool textureOK = false;
    bool initialized = false;
    bool animating = false;

    float lastTouchX, lastTouchY;
    bool touching = false;
};

static void initializeVulkan(Engine* engine) {
    if (!loadVulkanLibrary()) {
        LOGE("Vulkan is unavailable, install vulkan and re-start");
        return;
    }

    engine->inst = VulkanInstance(false);
    engine->inst.attach(engine->app->window);
    engine->inst.initializeDevice();

    int width = ANativeWindow_getWidth(engine->app->window);
    int height = ANativeWindow_getHeight(engine->app->window);

    engine->chain = new VulkanSwapchain(engine->inst);
    engine->chain->initalize(width, height, true);

    auto vertShaderCode = readAsset(engine->app->activity->assetManager, "shaders/cube.vert.spv");
    auto fragShaderCode = readAsset(engine->app->activity->assetManager, "shaders/cube.frag.spv");

    engine->gpipe = new VulkanGraphicsPipeline(*(engine->chain), vertShaderCode, fragShaderCode);
    engine->gpipe->enableTexture();
    engine->gpipe->enableUniformBuffer();

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

    std::vector<VkDescriptorSetLayoutBinding> bindings = {samplerLayoutBinding, uboLayoutBinding, modelLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(engine->inst.device, &layoutInfo, nullptr, &engine->descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
    engine->gpipe->setDescriptorSetLayout(engine->descriptorSetLayout);

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 8;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[3];
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[1].offset = sizeof(float)*3;
    attrs[2].location = 2; attrs[2].binding = 0; attrs[2].format = VK_FORMAT_R32G32_SFLOAT;  attrs[2].offset = sizeof(float)*6;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attrs;

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

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    engine->gpipe->create(pipelineInfo);

    int texW = 1, texH = 1;
    std::vector<unsigned char> pixels;
    engine->textureOK = true;
    try {
        pixels = load_png_rgba_asset(engine->app->activity->assetManager, "sample.png", texW, texH);
        if (pixels.empty()) throw std::runtime_error("empty pixels");
    } catch (...) {
        engine->textureOK = false;
        pixels = std::vector<unsigned char>(4, 255);
    }

    engine->smodel.init(engine->inst);
    engine->smodel.camera.aspect = (float)width / (float)height;
    engine->smodel.addLight(Vec4(4.0f, 4.0f, 4.0f, 0.0f), Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    engine->smodel.addLight(Vec4(-4.0f, -4.0f, -4.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 0.0f));
    engine->smodel.addLight(Vec4(0.0f, 0.0f, -4.0f, 0.0f), Vec4(0.0f, 0.0f, 1.0f, 0.0f));

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t kMaxSets = MAX_FRAMES_IN_FLIGHT * 16;
    poolSizes[0].descriptorCount = kMaxSets;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = kMaxSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = kMaxSets;
    if (vkCreateDescriptorPool(engine->inst.device, &poolInfo, nullptr, &engine->descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
    gDescriptorPool = engine->descriptorPool;
    gDescriptorSetLayout = engine->descriptorSetLayout;

    if (engine->textureOK) {
        engine->tmpTex = VulkanTexture(engine->inst);
        engine->tmpTex.load(pixels.data(), texW, texH);
    }

    auto lightMesh = createBoxMesh(0.5f, 0.5f, 0.5f);
    engine->light0.init(engine->inst, lightMesh.vertices, lightMesh.indices);
    engine->light1.init(engine->inst, lightMesh.vertices, lightMesh.indices);
    engine->light2.init(engine->inst, lightMesh.vertices, lightMesh.indices);
    engine->light0.material.color = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    engine->light1.material.color = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    engine->light2.material.color = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    engine->light0.material.createTextureFromColor(engine->inst);
    engine->light1.material.createTextureFromColor(engine->inst);
    engine->light2.material.createTextureFromColor(engine->inst);
    engine->light0.setPosition(Vec3(4.0f, 4.0f, 4.0f));
    engine->light1.setPosition(Vec3(-4.0f, -4.0f, -4.0f));
    engine->light2.setPosition(Vec3(0.0f, 0.0f, -4.0f));

    auto cubeMesh = createBoxMesh(2.0f, 2.0f, 2.0f);
    engine->model.init(engine->inst, cubeMesh.vertices, cubeMesh.indices);
    if (engine->textureOK) {
        engine->model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        engine->model.material.texture = engine->tmpTex;
    } else {
        engine->model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        engine->model.material.createTextureFromColor(engine->inst);
    }

    engine->renderer = new VulkanRenderer(*(engine->chain));
    engine->renderer->initialize(engine->inst);

    engine->initialized = true;
}

static void termVulkan(Engine* engine) {
    if (!engine->initialized) return;
    vkDeviceWaitIdle(engine->inst.device);
    engine->renderer->destroy();
    delete engine->renderer;
    engine->gpipe->destroy();
    delete engine->gpipe;
    engine->model.destroy();
    engine->light0.destroy();
    engine->light1.destroy();
    engine->light2.destroy();
    engine->smodel.destroy();
    if (engine->textureOK) engine->tmpTex.destroy();
    vkDestroyDescriptorPool(engine->inst.device, engine->descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engine->inst.device, engine->descriptorSetLayout, nullptr);
    engine->chain->destroy();
    delete engine->chain;
    engine->inst.destroy();
    engine->initialized = false;
}

static void drawFrame(Engine* engine) {
    if (!engine->initialized) return;

    VkClearValue clears[2];
    clears[0] = {{{0.3f, 0.3f, 0.3f, 1.0f}}};
    clears[1].depthStencil = {1.0f, 0};
    VkCommandBuffer cmd = engine->renderer->begin(*(engine->gpipe), clears, 2);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) engine->chain->swapChainExtent.width;
    viewport.height = (float) engine->chain->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = engine->chain->swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    engine->smodel.updateGlobal(engine->renderer->currentFrame);
    engine->model.render(cmd, engine->inst.device, engine->smodel, *(engine->gpipe), engine->renderer->currentFrame);
    engine->light0.render(cmd, engine->inst.device, engine->smodel, *(engine->gpipe), engine->renderer->currentFrame);
    engine->light1.render(cmd, engine->inst.device, engine->smodel, *(engine->gpipe), engine->renderer->currentFrame);
    engine->light2.render(cmd, engine->inst.device, engine->smodel, *(engine->gpipe), engine->renderer->currentFrame);
    engine->renderer->end();
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        if (action == AMOTION_EVENT_ACTION_DOWN) {
            engine->touching = true;
            engine->lastTouchX = x;
            engine->lastTouchY = y;
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            engine->touching = false;
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            float dx = x - engine->lastTouchX;
            float dy = y - engine->lastTouchY;
            engine->lastTouchX = x;
            engine->lastTouchY = y;
            engine->smodel.camera.orbit(dx * 0.005f, dy * 0.005f);
        }
        return 1;
    }
    return 0;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    Engine* engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                initializeVulkan(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            termVulkan(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->animating = false;
            break;
    }
}

void android_main(struct android_app* state) {
    Engine engine = {};
    engine.app = state;
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    state->onInputEvent = handleInput;

    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                termVulkan(&engine);
                return;
            }
        }

        if (engine.animating && engine.initialized) {
            drawFrame(&engine);
        }
    }
}
