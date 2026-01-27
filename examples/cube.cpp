#include "vkApp.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <cstring>
#include <array>
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics_math.h"

#define WIDTH 1280
#define HEIGHT 720

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static std::vector<unsigned char> load_png_rgba(const char* filename, int &outW, int &outH) {
    int n;
    unsigned char* data = stbi_load(filename, &outW, &outH, &n, STBI_rgb_alpha);
    if (!data) {
        throw std::runtime_error("failed to load image with stb_image");
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
        fov = 45.f * (float)M_PI / 180.0f;
        aspect = (float)WIDTH/(float)HEIGHT;
        nearClip = 0.1f;
        farClip = 100.0f;
        yaw = std::atan2(position.z, position.x);
        pitch = std::asin(position.y / position.length());
    }
    void orbit(float nx, float ny) {
        float sens = 1.0f;
        yaw += nx * sens;
        pitch += ny * sens;
        float limit = 89.0f * (float)M_PI / 180.0f;
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
        position = position + (-dirN) * ny;
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

void main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    bool debug_app = true;
    VulkanInstance inst(debug_app);
    inst.attach(window);
    inst.initializeDevice();
    VulkanSwapchain chain(inst);
    chain.initalize(WIDTH, HEIGHT, true);
    auto vertShaderCode = readFile("cube.vert.spv");
    auto fragShaderCode = readFile("cube.frag.spv");
    VulkanGraphicsPipeline gpipe(chain, vertShaderCode, fragShaderCode);
    gpipe.enableTexture();
    gpipe.enableUniformBuffer();

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
    VkDescriptorSetLayout descriptorSetLayout{};
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
    if (vkCreateDescriptorSetLayout(inst.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout in main");
    }
    gpipe.setDescriptorSetLayout(descriptorSetLayout);

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
    gpipe.create(pipelineInfo);

    int texW = 1, texH = 1;
    std::vector<unsigned char> pixels;
    bool textureOK = true;
    try {
        pixels = load_png_rgba("texture.jpg", texW, texH);
    } catch (...) {
        textureOK = false;
        pixels = std::vector<unsigned char>(4, 255);
    }
    ShaderModel smodel;
    smodel.init(inst);
    smodel.addLight(Vec4(4.0f, 4.0f, 4.0f, 0.0f), Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    smodel.addLight(Vec4(-4.0f, -4.0f, -4.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 0.0f));
    smodel.addLight(Vec4(0.0f, 0.0f, -4.0f, 0.0f), Vec4(0.0f, 0.0f, 1.0f, 0.0f));
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t kMaxSets = MAX_FRAMES_IN_FLIGHT * 16;
    poolSizes[0].descriptorCount = kMaxSets;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = kMaxSets;
    VkDescriptorPool descriptorPool{};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = kMaxSets;
    if (vkCreateDescriptorPool(inst.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool in main");
    }
    gDescriptorPool = descriptorPool;
    VulkanTexture tmpTex;
    if (textureOK) {
        tmpTex = VulkanTexture(inst);
        tmpTex.load(pixels.data(), texW, texH);
    }

    gDescriptorSetLayout = descriptorSetLayout;
    ModelObject light0;
    ModelObject light1;
    ModelObject light2;
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

    ModelObject model;
    auto cubeMesh = createBoxMesh(2.0f, 2.0f, 2.0f);
    model.init(inst, cubeMesh.vertices, cubeMesh.indices);
    if (textureOK) {
        model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        model.material.texture = tmpTex;
    } else {
        model.material.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        model.material.createTextureFromColor(inst);
    }

    VulkanRenderer renderer(chain);
    renderer.initialize(inst);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        VkClearValue clears[2];
        clears[0] = {{{0.3f, 0.3f, 0.3f, 1.0f}}};
        clears[1].depthStencil = {1.0f, 0};
        VkCommandBuffer cmd = renderer.begin(gpipe, clears, 2);

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

        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        static double lx = cx;
        static double ly = cy;
        double dx = cx - lx;
        double dy = cy - ly;
        lx = cx;
        ly = cy;
        int lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        int rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
        if (lmb == GLFW_PRESS) {
            smodel.camera.orbit((float)dx * 0.005f, (float)-dy * 0.005f);
        }
        if (rmb == GLFW_PRESS) {
            smodel.camera.move((float)dx * 0.1f, (float)-dy * 0.07f);
        }

        float angle = static_cast<float>(glfwGetTime());
        smodel.updateGlobal(renderer.currentFrame);
        model.render(cmd, inst.device, smodel, gpipe, renderer.currentFrame);
        light0.render(cmd, inst.device, smodel, gpipe, renderer.currentFrame);
        light1.render(cmd, inst.device, smodel, gpipe, renderer.currentFrame);
        light2.render(cmd, inst.device, smodel, gpipe, renderer.currentFrame);
        renderer.end();
    }
    vkDeviceWaitIdle(inst.device);
    renderer.destroy();
    gpipe.destroy();
    model.destroy();
    light0.destroy();
    light1.destroy();
    light2.destroy();
    smodel.destroy();
    vkDestroyDescriptorPool(inst.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(inst.device, descriptorSetLayout, nullptr);
    chain.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();

}
