#include "vkApp.h"
#include <iostream>
#include "utils.h"

#define WIDTH 1280
#define HEIGHT 720

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
    chain.initalize(WIDTH, HEIGHT, true);

    auto vertShaderCode = readFile("simple.vert.spv");
    auto fragShaderCode = readFile("simple.frag.spv");

    VulkanGraphicsPipeline gpipe(chain, vertShaderCode, fragShaderCode);

    gpipe.create();

    VulkanRenderer renderer(chain);
    renderer.initialize(inst);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        VkClearValue clears[2];
        clears[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};
        VkCommandBuffer cmd = renderer.begin(gpipe, clears, 2);

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

        vkCmdDraw(cmd, 3, 1, 0, 0);
        renderer.end();
    }

    vkDeviceWaitIdle(inst.device);
    renderer.destroy();
    gpipe.destroy();
    chain.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
