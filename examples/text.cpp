#include "vkApp.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include "utils.h"

#define WIDTH 1280
#define HEIGHT 720

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

    void loadMetrics(const char* path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open font metrics file");
        }
        file.read(reinterpret_cast<char*>(&startChar), 1);
        file.read(reinterpret_cast<char*>(&rowPitch), 1);
        file.read(reinterpret_cast<char*>(&columnFactor), sizeof(float));
        file.read(reinterpret_cast<char*>(&rowFactor), sizeof(float));
        file.read(reinterpret_cast<char*>(charWidths.data()), 256 * sizeof(float));
        file.close();
    }

    void loadTexture(const char* path) {
        int w, h, n;
        unsigned char* data = stbi_load(path, &w, &h, &n, STBI_rgb_alpha);
        if (!data) {
            throw std::runtime_error("failed to load font texture");
        }
        texture.load(data, w, h);
        stbi_image_free(data);
    }

    float getTextWidth(const std::string& text) const {
        float width = 0.0f;
        for (char c : text) {
            width += charWidths[static_cast<uint8_t>(c)];
        }
        return width;
    }

    float getLineMaxWidth(const std::vector<std::string>& lines) const {
        float maxW = 0.0f;
        for (const auto& line : lines) {
            float w = getTextWidth(line);
            if (w > maxW) maxW = w;
        }
        return maxW;
    }
};

class TextView {
public:
    static const int ALIGN_CENTER = 0;
    static const int ALIGN_CENTER_RIGHT = 1;
    static const int ALIGN_CENTER_LEFT = 2;

    Font* font = nullptr;
    std::string text;
    float textSize = 0.05f;
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

    TextView(VulkanInstance& inst, Font* f) : font(f), vertexBuffer(inst), indexBuffer(inst) {}

    void setPosition(float x, float y) {
        posX = x;
        posY = y;
    }

    void setText(const std::string& newText) {
        text = newText;
        updateText();
    }

    void setTextSize(float size) {
        textSize = size;
        updateText();
    }

    void setConstraintWidth(float w) {
        constraintWidth = w;
        updateText();
    }

    void setAlignment(int align) {
        alignment = align;
        updateText();
    }

    void setAnimationScroll(bool enable) {
        animationScroll = enable;
        updateText();
    }

    void setColor(float r, float g, float b) {
        defaultColor[0] = r;
        defaultColor[1] = g;
        defaultColor[2] = b;
        updateText();
    }

    float getHeight(float aspect) const {
        return numLines * textSize * (aspect > 1.0f ? 1.0f : aspect);
    }

    float getWidth(float aspect) const {
        if (constraintWidth > 0.0f && animationScroll) {
            return constraintWidth;
        }
        if (aspect < 1.0f) {
            return maxLineWidth * 0.5f * textSize / aspect;
        }
        return maxLineWidth * 0.5f * textSize;
    }

    void updateText() {
        if (!font) return;

        std::string process = text;
        if (constraintWidth > 0.0f && !animationScroll) {
            process = wrapText(text);
        }

        std::vector<std::string> lines = splitLines(process);
        maxLineWidth = font->getLineMaxWidth(lines);
        numLines = static_cast<int>(lines.size());
        int drawCount = getDrawCharCount(process);

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

        float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        if (aspect < 1.0f) {
            textAnimWidth = maxLineWidth * 0.5f * textSize / aspect;
        } else {
            textAnimWidth = maxLineWidth * 0.5f * textSize;
        }
    }

    void updateAnimation(float dt) {
        if (animationScroll && textAnimWidth > constraintWidth) {
            deltax -= textAnimWidth * 0.1f * dt;
            if (deltax < -textAnimWidth * 2.0f) {
                deltax = constraintWidth;
            }
        }
    }

    void render(VkCommandBuffer cmd, VulkanGraphicsPipeline& gpipe, float aspect, uint32_t vw, uint32_t vh) {
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
            setScissorFromNDC(cmd, posX, posY, constraintWidth, h, vw, vh);
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

    void destroy() {
        if (vertexBuffer.buffer != VK_NULL_HANDLE) vertexBuffer.destroy();
        if (indexBuffer.buffer != VK_NULL_HANDLE) indexBuffer.destroy();
    }

private:
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

    int getDrawCharCount(const std::string& s) const {
        int count = 0;
        for (char c : s) {
            if (c != '\n' && c != ' ') count++;
        }
        return count;
    }

    std::string wrapText(const std::string& src) {
        std::string result;
        float width = 0.0f;
        std::vector<std::string> lines = splitLines(src);
        for (size_t li = 0; li < lines.size(); ++li) {
            const std::string& line = lines[li];
            std::vector<std::string> words = splitWords(line);
            if (words.size() > 1) {
                for (size_t i = 0; i < words.size(); ++i) {
                    float w = getTextWidthReal(words[i]);
                    if ((width + w) > constraintWidth) {
                        if (!result.empty()) result += '\n';
                        result += words[i];
                        if (i < words.size() - 1) result += ' ';
                        width = w + (i < words.size() - 1 ? getTextWidthReal(" ") : 0.0f);
                    } else {
                        if (!result.empty() && result.back() != '\n') result += ' ';
                        result += words[i];
                        width += w + (i < words.size() - 1 ? getTextWidthReal(" ") : 0.0f);
                    }
                }
            } else if (words.size() == 1) {
                for (size_t i = 0; i < words[0].length(); ++i) {
                    std::string sampler(1, words[0][i]);
                    float w = getTextWidthReal(sampler);
                    if ((width + w) > constraintWidth) {
                        if (!result.empty()) result += '\n';
                        result += sampler;
                        width = w;
                    } else {
                        result += sampler;
                        width += w;
                    }
                }
            }
            if (li + 1 < lines.size()) {
                result += '\n';
                width = 0.0f;
            }
        }
        return result;
    }

    float getTextWidthReal(const std::string& t) const {
        return font->getTextWidth(t) * 0.5f * textSize;
    }

    static std::vector<std::string> splitWords(const std::string& s) {
        std::vector<std::string> words;
        size_t start = 0;
        while (true) {
            size_t end = s.find(' ', start);
            if (end == std::string::npos) {
                if (start < s.length()) words.push_back(s.substr(start));
                break;
            }
            if (start < end) words.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return words;
    }

    static void setScissorFromNDC(VkCommandBuffer cmd, float cx, float cy, float w, float h, uint32_t vw, uint32_t vh) {
        float left = cx - w * 0.5f;
        float top = cy + h * 0.5f;
        float right = cx + w * 0.5f;
        float bottom = cy - h * 0.5f;
        int32_t x = static_cast<int32_t>((left + 1.0f) * 0.5f * static_cast<float>(vw));
        int32_t y = static_cast<int32_t>((1.0f - top) * 0.5f * static_cast<float>(vh));
        int32_t pixW = static_cast<int32_t>((right - left) * 0.5f * static_cast<float>(vw));
        int32_t pixH = static_cast<int32_t>((top - bottom) * 0.5f * static_cast<float>(vh));
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (pixW < 0) pixW = 0;
        if (pixH < 0) pixH = 0;
        VkRect2D scissor{};
        scissor.offset = {x, y};
        scissor.extent = {static_cast<uint32_t>(pixW), static_cast<uint32_t>(pixH)};
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Text", nullptr, nullptr);

    bool debug_app = true;
    VulkanInstance inst(debug_app);
    inst.attach(window);
    inst.initializeDevice();

    VulkanSwapchain chain(inst);
    chain.initalize(WIDTH, HEIGHT, false);

    auto vertShaderCode = readFile("text.vert.spv");
    auto fragShaderCode = readFile("text.frag.spv");

    VulkanGraphicsPipeline gpipe(chain, vertShaderCode, fragShaderCode);
    gpipe.enableTexture();

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(TextVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputBindingDescription> bindings = {bindingDescription};
    std::vector<VkVertexInputAttributeDescription> attributes(3);
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof(TextVertex, pos);
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof(TextVertex, uv);
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(TextVertex, color);

    gpipe.setVertexInput(bindings, attributes);
    gpipe.enableAlphaBlending();
    gpipe.create();

    Font font(inst);
    font.loadMetrics("windows.fft");
    font.loadTexture("font_texture.png");

    gpipe.createTextureDescriptor(font.texture);

    TextView title(inst, &font);
    title.setText("Hello Vulkan Text!");
    title.setTextSize(0.1f);
    title.setAlignment(TextView::ALIGN_CENTER);
    title.setColor(1.0f, 1.0f, 0.0f);
    title.setPosition(0.0f, 0.5f);

    TextView centerText(inst, &font);
    centerText.setText("Center aligned text example\nwith multiple lines.");
    centerText.setTextSize(0.04f);
    centerText.setAlignment(TextView::ALIGN_CENTER);
    centerText.setPosition(0.0f, 0.1f);

    TextView leftText(inst, &font);
    leftText.setText("Left aligned text.");
    leftText.setTextSize(0.035f);
    leftText.setAlignment(TextView::ALIGN_CENTER_LEFT);
    leftText.setPosition(-0.5f, -0.3f);

    TextView rightText(inst, &font);
    rightText.setText("Right aligned text.");
    rightText.setTextSize(0.035f);
    rightText.setAlignment(TextView::ALIGN_CENTER_RIGHT);
    rightText.setPosition(0.5f, -0.3f);

    TextView scrollText(inst, &font);
    scrollText.setText("This is a scrolling text animation example that moves from right to left using scissor testing.");
    scrollText.setTextSize(0.04f);
    scrollText.setAlignment(TextView::ALIGN_CENTER);
    scrollText.setConstraintWidth(0.4f);
    scrollText.setAnimationScroll(true);
    scrollText.setPosition(0.0f, -0.6f);

    VulkanRenderer renderer(chain);
    renderer.initialize(inst);

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        scrollText.updateAnimation(dt);

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.2f, 1.0f}}};
        VkCommandBuffer cmd = renderer.begin(gpipe, &clearColor, 1);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(chain.swapChainExtent.width);
        viewport.height = static_cast<float>(chain.swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = chain.swapChainExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gpipe.pipelineLayout, 0, 1, &gpipe.descriptorSets[renderer.currentFrame], 0, nullptr);

        float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        uint32_t vw = chain.swapChainExtent.width;
        uint32_t vh = chain.swapChainExtent.height;

        title.render(cmd, gpipe, aspect, vw, vh);
        centerText.render(cmd, gpipe, aspect, vw, vh);
        leftText.render(cmd, gpipe, aspect, vw, vh);
        rightText.render(cmd, gpipe, aspect, vw, vh);
        scrollText.render(cmd, gpipe, aspect, vw, vh);

        renderer.end();
    }

    vkDeviceWaitIdle(inst.device);
    title.destroy();
    centerText.destroy();
    leftText.destroy();
    rightText.destroy();
    scrollText.destroy();
    font.texture.destroy();
    renderer.destroy();
    gpipe.destroy();
    chain.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
