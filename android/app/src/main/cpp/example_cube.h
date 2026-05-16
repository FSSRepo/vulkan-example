#ifndef EXAMPLE_CUBE_H
#define EXAMPLE_CUBE_H

#include "vk_android_example.h"
#include "shared_types.h"
#include "shared_utils.h"
#include "mesh_data.h"
#include "graphics_math.h"

class CubeExample : public IExample {
public:
    virtual void init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) override;
    virtual void cleanup(VulkanInstance& inst) override;
    virtual void draw(VulkanRenderer& renderer, uint32_t currentFrame) override;
    virtual void onTouch(float x, float y, int action, bool down) override;

private:
    VulkanGraphicsPipeline* gpipe = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    ShaderModel smodel;
    ModelObject light0, light1, light2;
    ModelObject model;
    VulkanTexture tmpTex;
    bool textureOK = false;

    float lastTouchX, lastTouchY;

    void createDescriptorSetLayout(VkDevice device);
    void createDescriptorPool(VkDevice device);
    void setupVertexInput();
};

#endif
