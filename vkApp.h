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

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

class VulkanSwapchain {
    private:
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    std::vector<VkImageView> swapChainImageViews;
    VulkanInstance inst;

    void createRenderpass();
    void createImageViews();
    void createFramebuffer();

    public:
    VulkanSwapchain(VulkanInstance instance);
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;
    VkSwapchainKHR swapChain;
    VkExtent2D swapChainExtent;

    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    void initalize(int default_width, int default_height, bool depth);
};

class VulkanGraphicsPipeline {
    private:
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VulkanSwapchain swapChain;
    VkPipelineLayout pipelineLayout;

    public:
    VkPipeline graphicsPipeline;
    VulkanGraphicsPipeline(VulkanSwapchain swap, const std::vector<char>& vertex, const std::vector<char>& fragment);
    void create(VkGraphicsPipelineCreateInfo & pipelineInfo);
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

    VulkanSwapchain& swapChain;

    public:
    uint32_t currentFrame = 0;
    uint32_t imageIndex;
    void initialize(VulkanInstance inst, VulkanSwapchain swap);
    VkCommandBuffer begin(VulkanGraphicsPipeline pipeline, VkClearValue* clearVals);
    void end();
};

class VulkanBuffer {
    private:
    public:
    void create(const void* data, int data_size, bool uniform_buffer) {}
};

class VulkanTexture {
    private:
    public:
    void load(const void* data, int width, int height) {}
};
#endif