#ifndef VK_ANDROID_EXAMPLE_H
#define VK_ANDROID_EXAMPLE_H

#include <android_native_app_glue.h>
#include "vulkan_wrapper.h"
#include "vkApp.h"

struct Engine;

class IExample {
public:
    virtual ~IExample() {}
    virtual void init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) = 0;
    virtual void cleanup(VulkanInstance& inst) = 0;
    virtual void draw(VulkanRenderer& renderer, uint32_t currentFrame) = 0;
    virtual void onTouch(float x, float y, int action, bool down) = 0;
};

extern Engine* g_currentEngine;

struct Engine {
    struct android_app* app;
    VulkanInstance inst;
    VulkanSwapchain* chain = nullptr;
    VulkanRenderer* renderer = nullptr;
    IExample* currentExample = nullptr;
    IExample* menuExample = nullptr;
    bool initialized = false;
    bool animating = false;
    int width = 0;
    int height = 0;
};

void androidSetExample(Engine* engine, IExample* example);
void androidReturnToMenu(Engine* engine);
IExample* createMenuExample();

#endif
