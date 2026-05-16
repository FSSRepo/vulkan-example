#ifndef EXAMPLE_MENU_H
#define EXAMPLE_MENU_H

#include "vk_android_example.h"
#include "shared_types.h"
#include "shared_utils.h"
#include "graphics_math.h"
#include "text_renderer.h"
#include <vector>
#include <string>

struct ExampleEntry {
    std::string name;
    IExample* (*create)();
};

class MenuExample : public IExample {
public:
    virtual void init(struct android_app* app, VulkanInstance& inst, VulkanSwapchain& chain, int width, int height) override;
    virtual void cleanup(VulkanInstance& inst) override;
    virtual void draw(VulkanRenderer& renderer, uint32_t currentFrame) override;
    virtual void onTouch(float x, float y, int action, bool down) override;

    void addExample(const ExampleEntry& entry);

private:
    struct android_app* app = nullptr;
    std::vector<ExampleEntry> examples;
    VulkanGraphicsPipeline* gpipe = nullptr;
    VulkanGraphicsPipeline* textPipe = nullptr;
    Font* font = nullptr;
    std::vector<TextView*> textViews;
    TextView* gpuNameText = nullptr;
    TextView* hintText = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;
    int selectedItem = -1;
    float lastTouchX, lastTouchY;
    bool touched = false;
    char gpuName[256];
    char hintMessage[128];

    int getTouchedItem(float x, float y, int width, int height);
    void setupTextPipeline(VulkanInstance& inst, VulkanSwapchain& chain);
    void updateHint(int index);
};

void registerExample(const std::string& name, IExample* (*create)());
IExample* createCubeExample();
IExample* createTextureExample();
IExample* createDepthExample();
IExample* createPostProcessExample();

#endif
