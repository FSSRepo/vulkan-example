
#ifdef ANDROID_VULKAN

#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
class VulkanInstance {
    private:
    VkSurfaceKHR surface;
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    public:
    VulkanInstance() {

    }
#ifdef ANDROID_VULKAN
    void attach();
#else
    void attach();
#endif
};

class vkSwapChain {
    private:
    public:
    vkSwapChain(VulkanInstance inst) {

    }
    void initalize(int default_width, int default_height, bool depth);
};

class vkGraphicsPipeline {
    private:
    public:
    vkGraphicsPipeline(VulkanInstance inst) {

    }
    void create();
};

class vkBuffer {
    private:
    public:
    void create(const void* data, int data_size, bool uniform_buffer);
};

class vkRenderer {
    private:
    public:
    void initialize(VulkanInstance inst);
    VkCommandPool begin();
    end();
};

class vkTexture {
    private:
    public:
    void load(const void* data, int width, int height);
};
