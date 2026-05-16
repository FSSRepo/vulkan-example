#ifndef __VULKAN_BUFFER_H__
#define __VULKAN_BUFFER_H__

#include "vkCommon.h"
#include "VulkanInstance.h"

class VulkanBuffer {
    private:
    VkDevice device{};
    VkPhysicalDevice physicalDevice{};
    VkQueue graphicsQueue{};
    VkCommandPool commandPool{};
    bool hostVisible = false;
    VkDeviceSize allocSize = 0;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    public:
    VkBuffer buffer{};
    VkDeviceMemory bufferMemory{};
    VkDeviceSize size{};
    VulkanBuffer() {}
    VulkanBuffer(VulkanInstance instance);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void create(const void* data, int data_size, bool uniform_buffer);
    void createIndex(const void* data, int data_size);
    void update(const void* data, int data_size);
    void destroy();
    ~VulkanBuffer();
};

#endif // __VULKAN_BUFFER_H__
