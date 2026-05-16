#ifndef EXAMPLE_POSTPROCESS_H
#define EXAMPLE_POSTPROCESS_H

#include "vk_android_example.h"
#include "shared_types.h"
#include "shared_utils.h"
#include "mesh_data.h"
#include "graphics_math.h"
#include <chrono>

class PostProcessExample : public IExample {
public:
    virtual void init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) override;
    virtual void cleanup(VulkanInstance& inst) override;
    virtual void draw(VulkanRenderer& renderer, uint32_t currentFrame) override;
    virtual void onTouch(float x, float y, int action, bool down) override;

private:
    struct android_app* app = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;

    VulkanGraphicsPipeline* scenePipe = nullptr;
    VulkanGraphicsPipeline* contrastPipe = nullptr;
    VulkanGraphicsPipeline* brightnessPipe = nullptr;
    VulkanGraphicsPipeline* blurPipe = nullptr;
    VulkanGraphicsPipeline* bloomFinalPipe = nullptr;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout bloomFinalLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool bloomPool = VK_NULL_HANDLE;

    VkFormat offFormat;
    VkImage offImage{};
    VkDeviceMemory offMemory{};
    VkImageView offView{};
    VkSampler offSampler{};
    VkRenderPass offRenderPass{};
    VkFramebuffer offFramebuffer{};
    VkImage offDepthImage{};
    VkDeviceMemory offDepthMemory{};
    VkImageView offDepthView{};
    VkFormat offDepthFormat;

    VkImage bloomImage1{}, bloomImage2{};
    VkDeviceMemory bloomMemory1{}, bloomMemory2{};
    VkImageView bloomView1{}, bloomView2{};
    VkFramebuffer bloomFB1{}, bloomFB2{};
    VkRenderPass bloomRenderPass{};

    VulkanBuffer rectVBuf;
    VulkanBuffer sceneVBuf;
    VulkanBuffer sceneIBuf;
    VulkanTexture srcTexture;
    VulkanTexture offAsTexture;
    VulkanTexture bloom1AsTex;
    VulkanTexture bloom2AsTex;

    VulkanBuffer sceneGUBO;
    VulkanBuffer sceneMUBO;

    VkDescriptorSet blurDS1[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet blurDS2[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet bloomFinalDS[MAX_FRAMES_IN_FLIGHT];

    int sceneIndexCount = 0;
    bool useBloom = false;
    std::chrono::steady_clock::time_point startTime;
    bool initialized = false;
    VkCommandPool tempCommandPool = VK_NULL_HANDLE;
    VkDevice savedDevice = VK_NULL_HANDLE;
    VkPhysicalDevice savedPhysicalDevice = VK_NULL_HANDLE;
    VkQueue savedGraphicsQueue = VK_NULL_HANDLE;

    void createOffscreenResources(VulkanInstance& inst, VulkanSwapchain& chain);
    void createBloomResources(VulkanInstance& inst, VulkanSwapchain& chain);
    void createSceneResources(VulkanInstance& inst, VulkanSwapchain& chain);
    void destroyOffscreenResources(VkDevice device);
    void destroyBloomResources(VkDevice device);
    VkCommandBuffer beginTempCommands();
    void endTempCommands(VkCommandBuffer cmd);
};

#endif
