#ifndef __VULKAN_INSTANCE_H__
#define __VULKAN_INSTANCE_H__

#include "vkCommon.h"

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

#endif // __VULKAN_INSTANCE_H__
