#include "VulkanBuffer.h"
#include "vkUtils.h"

VulkanBuffer::VulkanBuffer(VulkanInstance instance)
{
    device = instance.device;
    physicalDevice = instance.physicalDevice;
    graphicsQueue = instance.graphicsQueue;
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, instance.surface);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily.first;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer command pool!");
    }
}

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type for buffer!");
}

VkCommandBuffer VulkanBuffer::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanBuffer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanBuffer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize sz)
{
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = sz;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);
    endSingleTimeCommands(cmd);
}

void VulkanBuffer::create(const void *data, int data_size, bool uniform_buffer)
{
    size = static_cast<VkDeviceSize>(data_size);
    if (uniform_buffer)
    {
        hostVisible = true;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        VkDeviceSize align = props.limits.minUniformBufferOffsetAlignment;
        VkDeviceSize alignedSize = size;
        if (align > 0)
        {
            alignedSize = (size + align - 1) & ~(align - 1);
        }
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = alignedSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create uniform buffer!");
        }
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        uint32_t typeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = typeIndex;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate uniform buffer memory!");
        }
        if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to bind uniform buffer memory!");
        }
        allocSize = memRequirements.size;
        void *mapped;
        vkMapMemory(device, bufferMemory, 0, allocSize, 0, &mapped);
        std::memcpy(mapped, data, static_cast<size_t>(size));
        if (allocSize > size)
        {
            std::memset(static_cast<char *>(mapped) + static_cast<size_t>(size), 0, static_cast<size_t>(allocSize - size));
        }
        vkUnmapMemory(device, bufferMemory);
        return;
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create staging buffer!");
    }
    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingReq);
    uint32_t hostTypeIndex = findMemoryType(stagingReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAlloc{};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = hostTypeIndex;
    if (vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingBufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate staging buffer memory!");
    }
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
    void *mapped;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingBufferMemory);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vertex buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    uint32_t deviceTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = deviceTypeIndex;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to bind vertex buffer memory!");
    }

    VkCommandBufferAllocateInfo allocInfoCmd{};
    allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfoCmd.commandPool = commandPool;
    allocInfoCmd.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfoCmd, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanBuffer::createIndex(const void *data, int data_size)
{
    size = static_cast<VkDeviceSize>(data_size);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create staging buffer!");
    }
    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingReq);
    uint32_t hostTypeIndex = findMemoryType(stagingReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAlloc{};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = hostTypeIndex;
    if (vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingBufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate staging buffer memory!");
    }
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
    void *mapped;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingBufferMemory);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create index buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    uint32_t deviceTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = deviceTypeIndex;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate index buffer memory!");
    }
    if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to bind index buffer memory!");
    }

    VkCommandBufferAllocateInfo allocInfoCmd{};
    allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfoCmd.commandPool = commandPool;
    allocInfoCmd.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfoCmd, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanBuffer::update(const void *data, int data_size)
{
    VkDeviceSize sz = static_cast<VkDeviceSize>(data_size);
    if (hostVisible)
    {
        void *mapped;
        vkMapMemory(device, bufferMemory, 0, allocSize ? allocSize : sz, 0, &mapped);
        std::memcpy(mapped, data, static_cast<size_t>(std::min<VkDeviceSize>(sz, allocSize ? allocSize : sz)));
        if (allocSize && allocSize > sz)
        {
            std::memset(static_cast<char *>(mapped) + static_cast<size_t>(sz), 0, static_cast<size_t>(allocSize - sz));
        }
        vkUnmapMemory(device, bufferMemory);
        return;
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = sz;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create staging buffer");
    }
    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingReq);
    uint32_t hostTypeIndex = findMemoryType(stagingReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAlloc{};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = hostTypeIndex;
    if (vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingBufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate staging buffer memory");
    }
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
    void *mapped;
    vkMapMemory(device, stagingBufferMemory, 0, sz, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(sz));
    vkUnmapMemory(device, stagingBufferMemory);
    copyBuffer(stagingBuffer, buffer, sz);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanBuffer::destroy()
{
    if (buffer)
    {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (bufferMemory)
    {
        vkFreeMemory(device, bufferMemory, nullptr);
        bufferMemory = VK_NULL_HANDLE;
    }
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : device(other.device), physicalDevice(other.physicalDevice), graphicsQueue(other.graphicsQueue),
      commandPool(other.commandPool), hostVisible(other.hostVisible), allocSize(other.allocSize),
      buffer(other.buffer), bufferMemory(other.bufferMemory), size(other.size)
{
    other.device = VK_NULL_HANDLE;
    other.physicalDevice = VK_NULL_HANDLE;
    other.graphicsQueue = VK_NULL_HANDLE;
    other.commandPool = VK_NULL_HANDLE;
    other.hostVisible = false;
    other.allocSize = 0;
    other.buffer = VK_NULL_HANDLE;
    other.bufferMemory = VK_NULL_HANDLE;
    other.size = 0;
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        if (commandPool)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
        device = other.device;
        physicalDevice = other.physicalDevice;
        graphicsQueue = other.graphicsQueue;
        commandPool = other.commandPool;
        hostVisible = other.hostVisible;
        allocSize = other.allocSize;
        buffer = other.buffer;
        bufferMemory = other.bufferMemory;
        size = other.size;
        other.device = VK_NULL_HANDLE;
        other.physicalDevice = VK_NULL_HANDLE;
        other.graphicsQueue = VK_NULL_HANDLE;
        other.commandPool = VK_NULL_HANDLE;
        other.hostVisible = false;
        other.allocSize = 0;
        other.buffer = VK_NULL_HANDLE;
        other.bufferMemory = VK_NULL_HANDLE;
        other.size = 0;
    }
    return *this;
}

VulkanBuffer::~VulkanBuffer()
{
    destroy();
    if (commandPool)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
}
