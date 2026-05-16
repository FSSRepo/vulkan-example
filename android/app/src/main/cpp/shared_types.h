#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include "vulkan_wrapper.h"
#include "vkApp.h"
#include "graphics_math.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

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

struct Material {
    Vec4 color;
    VulkanTexture texture;

    void createTextureFromColor(VulkanInstance &inst) {
        texture = VulkanTexture(inst);
        unsigned char px[4];
        auto toByte = [](float c) {
            float v = std::max(0.0f, std::min(1.0f, c));
            return static_cast<unsigned char>(v * 255.0f);
        };
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
    std::vector<VkDescriptorSet> descriptorSets;
    int vertexCount = 0;
    int indexCount = 0;

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

    void setupDescriptors(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, ShaderModel &smodel) {
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
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

            std::array<VkWriteDescriptorSet, 3> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = descriptorSets[f];
            writes[0].dstBinding = 1;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo = &globalInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = descriptorSets[f];
            writes[1].dstBinding = 0;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &imageInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = descriptorSets[f];
            writes[2].dstBinding = 2;
            writes[2].dstArrayElement = 0;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[2].descriptorCount = 1;
            writes[2].pBufferInfo = &modelInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void setPosition(const Vec3 &p) {
        _pos = p;
        updateMatrix();
    }

    void setRotation(const Vec3 &r) {
        _rot = r;
        updateMatrix();
    }

    void render(VkCommandBuffer cmd, ShaderModel &smodel, VulkanGraphicsPipeline &gpipe, uint32_t currentFrame) {
        std::memcpy(mdata.model, modelMatrix.data(), sizeof(mdata.model));
        modelUbo[currentFrame].update(&mdata, sizeof(mdata));

        VkDeviceSize offsets[] = {0};
        VkBuffer vb = buffer.buffer;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpipe.pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }

    void destroy() {
        buffer.destroy();
        indexBuffer.destroy();
        material.destroy();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            modelUbo[i].destroy();
        }
    }

private:
    Vec3 _pos{0.0f, 0.0f, 0.0f};
    Vec3 _rot{0.0f, 0.0f, 0.0f};

    void updateMatrix() {
        Mat4 T = Mat4::translate(Mat4::identity(), _pos);
        Mat4 Rx = Mat4::rotate(Mat4::identity(), _rot.x, Vec3(1,0,0));
        Mat4 Ry = Mat4::rotate(Mat4::identity(), _rot.y, Vec3(0,1,0));
        Mat4 Rz = Mat4::rotate(Mat4::identity(), _rot.z, Vec3(0,0,1));
        modelMatrix = T * (Rz * Ry * Rx);
    }
};

#endif
