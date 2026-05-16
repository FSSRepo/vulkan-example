#include "text_renderer.h"
#include "shared_utils.h"
#include <android/log.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#define TAG "Text-Renderer"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

void Font::loadMetrics(AAssetManager* mgr, const std::string& path) {
    std::vector<char> data = readAsset(mgr, path);
    if (data.empty()) {
        LOGE("failed to open font metrics: %s", path.c_str());
        return;
    }
    size_t offset = 0;
    startChar = static_cast<uint8_t>(data[offset++]);
    rowPitch = static_cast<uint8_t>(data[offset++]);
    std::memcpy(&columnFactor, data.data() + offset, sizeof(float));
    offset += sizeof(float);
    std::memcpy(&rowFactor, data.data() + offset, sizeof(float));
    offset += sizeof(float);
    std::memcpy(charWidths.data(), data.data() + offset, 256 * sizeof(float));
}

void Font::loadTexture(AAssetManager* mgr, const std::string& path) {
    int w, h;
    std::vector<unsigned char> pixels = loadPngRgbaAsset(mgr, path.c_str(), w, h);
    if (pixels.empty()) {
        LOGE("failed to load font texture: %s", path.c_str());
        return;
    }
    texture.load(pixels.data(), w, h);
}

float Font::getTextWidth(const std::string& text) const {
    float width = 0.0f;
    for (char c : text) {
        width += charWidths[static_cast<uint8_t>(c)];
    }
    return width;
}

float Font::getLineMaxWidth(const std::vector<std::string>& lines) const {
    float maxW = 0.0f;
    for (const auto& line : lines) {
        float w = getTextWidth(line);
        if (w > maxW) maxW = w;
    }
    return maxW;
}

TextView::TextView(VulkanInstance& inst, Font* f) : font(f), vertexBuffer(inst), indexBuffer(inst) {}

void TextView::setPosition(float x, float y) {
    posX = x;
    posY = y;
}

void TextView::setText(const std::string& newText) {
    text = newText;
    updateText(0, 0);
}

void TextView::setTextSize(float size) {
    textSize = size;
    updateText(0, 0);
}

void TextView::setConstraintWidth(float w) {
    constraintWidth = w;
    updateText(0, 0);
}

void TextView::setAlignment(int align) {
    alignment = align;
    updateText(0, 0);
}

void TextView::setAnimationScroll(bool enable) {
    animationScroll = enable;
    updateText(0, 0);
}

void TextView::setColor(float r, float g, float b) {
    defaultColor[0] = r;
    defaultColor[1] = g;
    defaultColor[2] = b;
    rebuild();
}

void TextView::setColorNoRebuild(float r, float g, float b) {
    defaultColor[0] = r;
    defaultColor[1] = g;
    defaultColor[2] = b;
}

float TextView::getHeight(float aspect) const {
    return numLines * textSize * (aspect > 1.0f ? 1.0f : aspect);
}

float TextView::getWidth(float aspect) const {
    if (constraintWidth > 0.0f && animationScroll) {
        return constraintWidth;
    }
    if (aspect < 1.0f) {
        return maxLineWidth * 0.5f * textSize / aspect;
    }
    return maxLineWidth * 0.5f * textSize;
}

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t end = s.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

void TextView::rebuild() {
    if (!font) return;

    std::vector<std::string> lines = splitLines(text);
    int drawCount = 0;
    for (char c : text) {
        if (c != '\n' && c != ' ') drawCount++;
    }

    std::vector<TextVertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(drawCount * 4);
    indices.reserve(drawCount * 6);

    float cursorX = 0.0f;
    float cursorY = static_cast<float>(numLines - 1);

    for (const auto& line : lines) {
        switch (alignment) {
            case ALIGN_CENTER:
                cursorX = -font->getTextWidth(line) * 0.5f;
                break;
            case ALIGN_CENTER_RIGHT:
                cursorX = (maxLineWidth * 0.5f) - font->getTextWidth(line);
                break;
            case ALIGN_CENTER_LEFT:
                cursorX = maxLineWidth * -0.5f;
                break;
        }

        for (char c : line) {
            uint8_t cc = static_cast<uint8_t>(c);
            if (c != ' ') {
                uint8_t row = (cc - font->startChar) / font->rowPitch;
                uint8_t col = (cc - font->startChar) - (font->rowPitch * row);
                float u_start = col * font->columnFactor;
                float v_start = row * font->rowFactor;
                float u_end = u_start + font->columnFactor;
                float v_end = v_start + font->rowFactor;

                uint16_t base = static_cast<uint16_t>(vertices.size());
                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 3);
                indices.push_back(base);
                indices.push_back(base + 2);
                indices.push_back(base + 3);

                TextVertex v0 = {{cursorX, cursorY + 1.0f}, {u_start, v_start}, {defaultColor[0], defaultColor[1], defaultColor[2]}};
                TextVertex v1 = {{cursorX, cursorY - 1.0f}, {u_start, v_end},   {defaultColor[0], defaultColor[1], defaultColor[2]}};
                TextVertex v2 = {{cursorX + 1.0f, cursorY + 1.0f}, {u_end, v_start}, {defaultColor[0], defaultColor[1], defaultColor[2]}};
                TextVertex v3 = {{cursorX + 1.0f, cursorY - 1.0f}, {u_end, v_end},   {defaultColor[0], defaultColor[1], defaultColor[2]}};
                vertices.push_back(v0);
                vertices.push_back(v1);
                vertices.push_back(v2);
                vertices.push_back(v3);
            }
            cursorX += font->charWidths[cc] * 1.0f;
        }
        cursorY -= 2.0f;
    }

    if (vertexBuffer.buffer != VK_NULL_HANDLE) vertexBuffer.destroy();
    if (indexBuffer.buffer != VK_NULL_HANDLE) indexBuffer.destroy();

    if (!vertices.empty()) {
        vertexBuffer.create(vertices.data(), static_cast<int>(vertices.size() * sizeof(TextVertex)), false);
    }
    if (!indices.empty()) {
        indexBuffer.createIndex(indices.data(), static_cast<int>(indices.size() * sizeof(uint16_t)));
    }
    indexCount = static_cast<int>(indices.size());
    buffersValid = !vertices.empty();
}

void TextView::updateText(float screenWidth, float screenHeight) {
    if (!font) return;

    std::vector<std::string> lines = splitLines(text);
    maxLineWidth = font->getLineMaxWidth(lines);
    numLines = static_cast<int>(lines.size());

    rebuild();

    float aspect = (screenHeight > 0.0f) ? (screenWidth / screenHeight) : 1.0f;
    if (aspect < 1.0f && aspect > 0.0f) {
        textAnimWidth = maxLineWidth * 0.5f * textSize / aspect;
    } else {
        textAnimWidth = maxLineWidth * 0.5f * textSize;
    }
}

void TextView::updateAnimation(float dt) {
    if (animationScroll && textAnimWidth > constraintWidth) {
        deltax -= textAnimWidth * 0.1f * dt;
        if (deltax < -textAnimWidth * 2.0f) {
            deltax = constraintWidth;
        }
    }
}

void TextView::render(VkCommandBuffer cmd, VulkanGraphicsPipeline& gpipe, float aspect, uint32_t vw, uint32_t vh) {
    if (!buffersValid || !font) return;

    float sx, sy;
    if (aspect > 1.0f) {
        sx = textSize * 0.5f;
        sy = textSize * 0.5f;
    } else {
        sx = textSize * 0.5f / aspect;
        sy = textSize * 0.5f * aspect;
    }

    bool useScroll = animationScroll && (textAnimWidth > constraintWidth);

    PushConstants pc;
    pc.offset[0] = 0.0f;
    pc.offset[1] = 0.0f;

    if (useScroll) {
        float animX = (posX - constraintWidth) + textAnimWidth + deltax;
        pc.position[0] = animX;
        pc.position[1] = posY;

        float h = getHeight(aspect);
        float left = posX - constraintWidth * 0.5f;
        float top = posY + h * 0.5f;
        float right = posX + constraintWidth * 0.5f;
        float bottom = posY - h * 0.5f;
        int32_t sx = static_cast<int32_t>((left + 1.0f) * 0.5f * static_cast<float>(vw));
        int32_t sy = static_cast<int32_t>((1.0f - top) * 0.5f * static_cast<float>(vh));
        int32_t sw = static_cast<int32_t>((right - left) * 0.5f * static_cast<float>(vw));
        int32_t sh = static_cast<int32_t>((top - bottom) * 0.5f * static_cast<float>(vh));
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sw < 0) sw = 0;
        if (sh < 0) sh = 0;
        VkRect2D scissor{};
        scissor.offset = {sx, sy};
        scissor.extent = {static_cast<uint32_t>(sw), static_cast<uint32_t>(sh)};
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    } else {
        pc.position[0] = posX;
        pc.position[1] = posY;
    }
    pc.scale[0] = sx;
    pc.scale[1] = sy;

    vkCmdPushConstants(cmd, gpipe.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    VkBuffer vb[] = { vertexBuffer.buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

    if (useScroll) {
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {vw, vh};
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }
}

void TextView::destroy() {
    if (vertexBuffer.buffer != VK_NULL_HANDLE) vertexBuffer.destroy();
    if (indexBuffer.buffer != VK_NULL_HANDLE) indexBuffer.destroy();
}
