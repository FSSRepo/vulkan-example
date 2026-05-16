#ifndef __VULKAN_TEXTURE_H__
#define __VULKAN_TEXTURE_H__

#include "vkCommon.h"
#include "VulkanInstance.h"
#include "VulkanBuffer.h"

class VulkanTexture {
    private:
    VkDevice device{};
    VkPhysicalDevice physicalDevice{};
    VkQueue graphicsQueue{};
    VkCommandPool commandPool{};
    VkImage image{};
    VkDeviceMemory imageMemory{};
    uint32_t imgWidth;
    uint32_t imgHeight;

    public:
    VkImageView imageView{};
    VkSampler sampler{};

    private:

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    public:
    VulkanTexture() {}
    VulkanTexture(VulkanInstance instance);
    void load(const void* data, int width, int height);
    void destroy();
    VkImageView getImageView() { return imageView; }
    VkSampler getSampler() { return sampler; }
};

#endif // __VULKAN_TEXTURE_H__
