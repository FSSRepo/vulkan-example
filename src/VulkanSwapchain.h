#ifndef __VULKAN_SWAPCHAIN_H__
#define __VULKAN_SWAPCHAIN_H__

#include "vkCommon.h"
#include "VulkanInstance.h"

class VulkanSwapchain {
    private:
    VkFormat swapChainImageFormat;
    std::vector<VkImageView> swapChainImageViews;
    VulkanInstance inst;

    void createRenderpass();
    void createImageViews();
    void createFramebuffer();
    void createDepthResources();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();

    public:
    VulkanSwapchain(VulkanInstance instance);
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;
    VkSwapchainKHR swapChain;
    VkExtent2D swapChainExtent;
    std::vector<VkImage> swapChainImages;

    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    bool useDepth = false;
    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};
    VkImageView depthImageView{};

    void initalize(int defaultWidth, int defaultHeight, bool depth);
    void destroy();
};

#endif // __VULKAN_SWAPCHAIN_H__
