#include "vkApp.h"
#include <iostream>
#include <cstddef>
#include "utils.h"

#define WIDTH 1280
#define HEIGHT 720

struct Vertex {
    float position[2];
    float uv[2];
};

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    bool debug_app = true;

    VulkanInstance inst(debug_app);
    inst.attach(window);
    inst.initializeDevice();

    VulkanSwapchain chain(inst);
    chain.initalize(WIDTH, HEIGHT, false);

    auto vertShaderCode = readFile("vertex.vert.spv");
    auto fragShaderCode = readFile("vertex.frag.spv");

    VulkanGraphicsPipeline gpipe(chain, vertShaderCode, fragShaderCode);

    std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {0.0f, 0.0f}},
        {{0.5f,  0.5f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 1.0f}}
    };

    VulkanBuffer vbuf(inst);
    vbuf.create(vertices.data(), static_cast<int>(vertices.size() * sizeof(Vertex)), false);

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputBindingDescription> bindings = {bindingDescription};
    std::vector<VkVertexInputAttributeDescription> attributes(2);
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = static_cast<uint32_t>(offsetof(Vertex, position));
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = static_cast<uint32_t>(offsetof(Vertex, uv));

    gpipe.setVertexInput(bindings, attributes);
    gpipe.create();

    VulkanRenderer renderer(chain);
    renderer.initialize(inst);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkCommandBuffer cmd = renderer.begin(gpipe, &clearColor, 1);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) chain.swapChainExtent.width;
        viewport.height = (float) chain.swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = chain.swapChainExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkBuffer buffers[] = { vbuf.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

        vkCmdDraw(cmd, 3, 1, 0, 0);
        renderer.end();
    }

    vkDeviceWaitIdle(inst.device);
    renderer.destroy();
    gpipe.destroy();
    chain.destroy();
    vbuf.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}