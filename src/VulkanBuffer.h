#ifndef __VULKAN_BUFFER_H__
#define __VULKAN_BUFFER_H__

#include "vkCommon.h"
#include "VulkanInstance.h"
#include "vkUtils.h"

class VulkanBuffer {
    private:
    VkDevice device{};
    VkPhysicalDevice physicalDevice{};
    VkQueue graphicsQueue{};
    VkCommandPool commandPool{};
    QueueFamilyIndices qFamilyIndices;

    bool hostVisible = false;
    VkDeviceSize allocSize = 0;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize* outAllocSize = nullptr);
    void mapCopyUnmap(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize dataSize, const void* data, VkDeviceSize clearExtra);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void createCommandPool();
    void ensureCommandPool();
    
    public:
    VkBuffer buffer{};
    VkDeviceMemory bufferMemory{};
    VkDeviceSize size{};
    VulkanBuffer() {}
    VulkanBuffer(VulkanInstance instance);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void create(const void* data, int dataSize, bool uniformBuffer);
    void createIndex(const void* data, int dataSize);
    void update(const void* data, int dataSize);
    void destroy();
    ~VulkanBuffer();
};

#endif // __VULKAN_BUFFER_H__
