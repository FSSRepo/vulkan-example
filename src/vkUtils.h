#ifndef __VK_UTILS_H__
#define __VK_UTILS_H__

#include "vkCommon.h"

template <typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

std::vector<const char*> getRequiredExtensions();

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
   std::pair<uint32_t, bool> graphicsFamily {0, false};
    std::pair<uint32_t, bool> presentFamily  {0, false};

    bool isComplete() const {
        return graphicsFamily.second && presentFamily.second;
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char *> device_extensions);
bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<const char *> exts);
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int default_width, int default_height);
VkShaderModule createShaderModule(VkDevice dev, const std::vector<char>& code);
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

#endif // __VK_UTILS_H__
