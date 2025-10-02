#include "vkApp.h"

#define WIDTH 1280
#define HEIGHT 720

void main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    VulkanInstance inst(true);
    inst.attach(window);
    inst.initializeDevice();
    
}