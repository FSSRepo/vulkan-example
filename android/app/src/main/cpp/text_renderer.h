#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "vk_android_example.h"
#include <vector>
#include <string>
#include <android/asset_manager.h>

struct PushConstants {
    float position[2];
    float scale[2];
    float offset[2];
};

struct TextVertex {
    float pos[2];
    float uv[2];
    float color[3];
};

class Font {
public:
    uint8_t startChar = 0;
    uint8_t rowPitch = 0;
    float columnFactor = 0.0f;
    float rowFactor = 0.0f;
    std::vector<float> charWidths;
    VulkanTexture texture;

    Font() {}
    Font(VulkanInstance& inst) : texture(inst) {
        charWidths.resize(256, 0.0f);
    }

    void loadMetrics(AAssetManager* mgr, const std::string& path);
    void loadTexture(AAssetManager* mgr, const std::string& path);

    float getTextWidth(const std::string& text) const;
    float getLineMaxWidth(const std::vector<std::string>& lines) const;
};

class TextView {
public:
    static const int ALIGN_CENTER = 0;
    static const int ALIGN_CENTER_RIGHT = 1;
    static const int ALIGN_CENTER_LEFT = 2;

    Font* font = nullptr;
    std::string text;
    float textSize = 0.035f;
    float constraintWidth = 0.0f;
    float maxLineWidth = 0.0f;
    int numLines = 0;
    int alignment = ALIGN_CENTER;
    bool animationScroll = false;
    float deltax = 0.0f;
    float textAnimWidth = 0.0f;
    float defaultColor[3] = {1.0f, 1.0f, 1.0f};
    float posX = 0.0f;
    float posY = 0.0f;

    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
    int indexCount = 0;
    bool buffersValid = false;

    TextView(VulkanInstance& inst, Font* f);
    void setPosition(float x, float y);
    void setText(const std::string& newText);
    void setTextSize(float size);
    void setConstraintWidth(float w);
    void setAlignment(int align);
    void setAnimationScroll(bool enable);
    void setColor(float r, float g, float b);

    float getHeight(float aspect) const;
    float getWidth(float aspect) const;

    void setColorNoRebuild(float r, float g, float b);
    void rebuild();

    void updateText(float screenWidth, float screenHeight);
    void updateAnimation(float dt);
    void render(VkCommandBuffer cmd, VulkanGraphicsPipeline& gpipe, float aspect, uint32_t vw, uint32_t vh);
    void destroy();
};

#endif
