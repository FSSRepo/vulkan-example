#ifndef __VK_APP__
#define __VK_APP__
#ifdef ANDROID_VULKAN
#include "vulkan_wrapper.h"
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <set>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <vector>
#include <stdexcept>

#define MAX_FRAMES_IN_FLIGHT 2

class VulkanInstance {
    private:
    VkInstance instance;
    std::vector<const char *> device_extensions;
    bool debugInstance;

    void createInstance(bool debug);

    public:
    VulkanInstance() {}
    VulkanInstance(bool debug);

#ifdef ANDROID_VULKAN
    void attach(ANativeWindow* window);
#else
    void attach(GLFWwindow* window);
#endif
    void initializeDevice();
    void destroy();

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

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

    void initalize(int default_width, int default_height, bool depth);
    void destroy();
};

class VulkanGraphicsPipeline {
    private:
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VulkanSwapchain swapChain;
    
    bool useTexture = false;
    uint32_t uniformBindingCount = 0;
    bool depthOverride = false;
    bool ownsDescriptorSetLayout = false;
    VkDescriptorPool descriptorPool{};

    public:
    VkDescriptorSetLayout descriptorSetLayout{};
    std::vector<VkDescriptorSet> descriptorSets;
    VkPipeline graphicsPipeline;
    VulkanGraphicsPipeline(VulkanSwapchain swap, const std::vector<char>& vertex, const std::vector<char>& fragment);

    void create(VkGraphicsPipelineCreateInfo & pipelineInfo);
    VkPipelineLayout pipelineLayout;
    void enableTexture();
    void enableUniformBuffer();
    void setUniformBufferBindingCount(uint32_t count);
    void enableDepthTest();
    void createTextureDescriptor(class VulkanTexture &texture);
    void createDescriptors(class VulkanBuffer &ubo, class VulkanTexture *texture);
    void setDescriptorSetLayout(VkDescriptorSetLayout layout);
    void setRenderPass(VkRenderPass renderPass);
    void destroy();
};

class VulkanRenderer {
    private:
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VulkanSwapchain swapChain;

    public:
    uint32_t currentFrame = 0;
    uint32_t imageIndex;

    VulkanRenderer(VulkanSwapchain swap) : swapChain(swap) {}
    void initialize(VulkanInstance inst);
    VkCommandBuffer begin(VulkanGraphicsPipeline pipeline, VkClearValue* clearVals, int clearCounts);
    void end();
    void destroy();
};

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
    VulkanBuffer(VulkanInstance inst);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void create(const void* data, int data_size, bool uniform_buffer);
    void createIndex(const void* data, int data_size);
    void update(const void* data, int data_size);
    void destroy();
};

class VulkanTexture {
    private:
    VkDevice device{};
    VkPhysicalDevice physicalDevice{};
    VkQueue graphicsQueue{};
    VkCommandPool commandPool{};
    uint32_t mipLevels = 1;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 4;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h);
    public:
    VkImage image{};
    VkDeviceMemory imageMemory{};
    VkImageView imageView{};
    VkSampler sampler{};
    VulkanTexture() {}
    VulkanTexture(VulkanInstance inst);
    void load(const void* data, int w, int h);
    void destroy();
};
#endif
