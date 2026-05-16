#ifndef __VULKAN_RENDERER_H__
#define __VULKAN_RENDERER_H__

#include "vkCommon.h"
#include "VulkanInstance.h"
#include "VulkanSwapchain.h"
#include "VulkanGraphicsPipeline.h"

class VulkanRenderer {
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    public:
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VulkanSwapchain swapChain;

    uint32_t currentFrame = 0;
    uint32_t imageIndex;

    VulkanRenderer(VulkanSwapchain swap) : swapChain(swap) {}
    void initialize(VulkanInstance instance);
    VkCommandBuffer begin(VulkanGraphicsPipeline pipeline, VkClearValue* clearVals, int clearCounts);
    void end();
    void destroy();
};

#endif // __VULKAN_RENDERER_H__
