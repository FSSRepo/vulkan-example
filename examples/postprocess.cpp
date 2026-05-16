#include "vkApp.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <cstddef>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics_math.h"

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

static std::vector<unsigned char> load_png_rgba(const char* filename, int &outW, int &outH) {
    int n;
    unsigned char* data = stbi_load(filename, &outW, &outH, &n, STBI_rgb_alpha);
    if (!data) {
        throw std::runtime_error("failed to load image with stb_image");
    }
    std::vector<unsigned char> pixels((size_t)outW * (size_t)outH * 4);
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);
    return pixels;
}

#define WIDTH 800
#define HEIGHT 600

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

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

    std::vector<char> vertShaderCode = readFile("contrast.vert.spv");
    std::vector<char> contrastFragCode = readFile("contrast.frag.spv");
    std::vector<char> brightnessFragCode = readFile("brightness.frag.spv");
    std::vector<char> blurFragCode = readFile("blur.frag.spv");
    std::vector<char> bloomFinalFragCode = readFile("bloom_final.frag.spv");

    VkFormat offFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImage offImage{};
    VkDeviceMemory offMemory{};
    VkImageView offView{};
    VkSampler offSampler{};
    VkRenderPass offRenderPass{};
    VkFramebuffer offFramebuffer{};
    VkImage offDepthImage{};
    VkDeviceMemory offDepthMemory{};
    VkImageView offDepthView{};
    VkFormat offDepthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = chain.swapChainExtent.width;
    imgInfo.extent.height = chain.swapChainExtent.height;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = offFormat;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.flags = 0;
    if (vkCreateImage(inst.device, &imgInfo, nullptr, &offImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen image");
    }
    VkMemoryRequirements offReq{};
    vkGetImageMemoryRequirements(inst.device, offImage, &offReq);
    VkMemoryAllocateInfo offAlloc{};
    offAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    offAlloc.allocationSize = offReq.size;
    offAlloc.memoryTypeIndex = findMemoryType(inst.physicalDevice, offReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(inst.device, &offAlloc, nullptr, &offMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate offscreen image memory");
    }
    if (vkBindImageMemory(inst.device, offImage, offMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("failed to bind offscreen image memory");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = offImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = offFormat;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(inst.device, &viewInfo, nullptr, &offView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen image view");
    }

    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.extent.width = chain.swapChainExtent.width;
    depthInfo.extent.height = chain.swapChainExtent.height;
    depthInfo.extent.depth = 1;
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.format = offDepthFormat;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.flags = 0;
    if (vkCreateImage(inst.device, &depthInfo, nullptr, &offDepthImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen depth image");
    }
    VkMemoryRequirements offDepthReq{};
    vkGetImageMemoryRequirements(inst.device, offDepthImage, &offDepthReq);
    VkMemoryAllocateInfo offDepthAlloc{};
    offDepthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    offDepthAlloc.allocationSize = offDepthReq.size;
    offDepthAlloc.memoryTypeIndex = findMemoryType(inst.physicalDevice, offDepthReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(inst.device, &offDepthAlloc, nullptr, &offDepthMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate offscreen depth image memory");
    }
    if (vkBindImageMemory(inst.device, offDepthImage, offDepthMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("failed to bind offscreen depth image memory");
    }

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = offDepthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = offDepthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(inst.device, &depthViewInfo, nullptr, &offDepthView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen depth view");
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = offFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = offDepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[2] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = atts;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;
    if (vkCreateRenderPass(inst.device, &rpInfo, nullptr, &offRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen render pass");
    }

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = offRenderPass;
    fbInfo.attachmentCount = 2;
    VkImageView attachments[2] = {offView, offDepthView};
    fbInfo.pAttachments = attachments;
    fbInfo.width = chain.swapChainExtent.width;
    fbInfo.height = chain.swapChainExtent.height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(inst.device, &fbInfo, nullptr, &offFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen framebuffer");
    }

    VkSamplerCreateInfo sampInfo{};
    sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;
    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.anisotropyEnable = VK_FALSE;
    sampInfo.maxAnisotropy = 1.0f;
    sampInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampInfo.unnormalizedCoordinates = VK_FALSE;
    sampInfo.compareEnable = VK_FALSE;
    sampInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampInfo.mipLodBias = 0.0f;
    sampInfo.minLod = 0.0f;
    sampInfo.maxLod = 0.0f;
    if (vkCreateSampler(inst.device, &sampInfo, nullptr, &offSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen sampler");
    }

    // Intermediate bloom resources
    VkImage bloomImage1{}, bloomImage2{};
    VkDeviceMemory bloomMemory1{}, bloomMemory2{};
    VkImageView bloomView1{}, bloomView2{};
    VkFramebuffer bloomFB1{}, bloomFB2{};
    VkRenderPass bloomRenderPass{};

    auto createBloomImage = [&](VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        VkImageCreateInfo info = imgInfo;
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (vkCreateImage(inst.device, &info, nullptr, &img) != VK_SUCCESS) throw std::runtime_error("bloom image");
        VkMemoryRequirements req; vkGetImageMemoryRequirements(inst.device, img, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = findMemoryType(inst.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(inst.device, &alloc, nullptr, &mem) != VK_SUCCESS) throw std::runtime_error("bloom mem");
        vkBindImageMemory(inst.device, img, mem, 0);
        VkImageViewCreateInfo vinfo = viewInfo;
        vinfo.image = img;
        if (vkCreateImageView(inst.device, &vinfo, nullptr, &view) != VK_SUCCESS) throw std::runtime_error("bloom view");
    };

    createBloomImage(bloomImage1, bloomMemory1, bloomView1);
    createBloomImage(bloomImage2, bloomMemory2, bloomView2);

    {
        VkAttachmentDescription att{};
        att.format = offFormat;
        att.samples = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &ref;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp.attachmentCount = 1;
        rp.pAttachments = &att;
        rp.subpassCount = 1;
        rp.pSubpasses = &sub;
        rp.dependencyCount = 1;
        rp.pDependencies = &dependency;
        if (vkCreateRenderPass(inst.device, &rp, nullptr, &bloomRenderPass) != VK_SUCCESS) throw std::runtime_error("bloom rp");
    }

    auto createBloomFB = [&](VkFramebuffer& fb, VkImageView v) {
        VkFramebufferCreateInfo fbi = fbInfo;
        fbi.renderPass = bloomRenderPass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &v;
        if (vkCreateFramebuffer(inst.device, &fbi, nullptr, &fb) != VK_SUCCESS) throw std::runtime_error("bloom fb");
    };
    createBloomFB(bloomFB1, bloomView1);
    createBloomFB(bloomFB2, bloomView2);

    struct ModelUniform { float model[16]; };

    struct Vertex { float position[2]; float uv[2]; };
    std::vector<Vertex> rectVerts = {
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    };
    VulkanBuffer vbuf(inst);
    vbuf.create(rectVerts.data(), static_cast<int>(rectVerts.size() * sizeof(Vertex)), false);

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2];
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = static_cast<uint32_t>(offsetof(Vertex, position));
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = static_cast<uint32_t>(offsetof(Vertex, uv));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 1;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding modelLayoutBinding{};
    modelLayoutBinding.binding = 2;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    modelLayoutBinding.pImmutableSamplers = nullptr;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    std::vector<VkDescriptorSetLayoutBinding> bindings = {samplerLayoutBinding, uboLayoutBinding, modelLayoutBinding};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(inst.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout in postprocess");
    }
    
    int texWidth, texHeight;
    std::vector<unsigned char> pixels;
    bool textureOK = true;
    try {
        pixels = load_png_rgba("texture.jpg", texWidth, texHeight);
    } catch (...) {
        textureOK = false;
        pixels = std::vector<unsigned char>(4, 255);
        texWidth = 1; texHeight = 1;
    }

    VulkanTexture srcTexture(inst);
    srcTexture.load(pixels.data(), texWidth, texHeight);

    
    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.pVertexInputState = &vertexInputInfo;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizer2{};
    rasterizer2.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer2.depthClampEnable = VK_FALSE;
    rasterizer2.rasterizerDiscardEnable = VK_FALSE;
    rasterizer2.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer2.lineWidth = 1.0f;
    rasterizer2.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer2.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer2.depthBiasEnable = VK_FALSE;
    pipeInfo.pRasterizationState = &rasterizer2;

    VulkanGraphicsPipeline contrastPipe(chain, vertShaderCode, contrastFragCode);
    contrastPipe.enableTexture();
    contrastPipe.create(pipeInfo);

    VulkanGraphicsPipeline brightnessPipe(chain, vertShaderCode, brightnessFragCode);
    brightnessPipe.enableTexture();
    brightnessPipe.setRenderPass(bloomRenderPass);
    brightnessPipe.create(pipeInfo);

    VulkanGraphicsPipeline blurPipe(chain, vertShaderCode, blurFragCode);
    blurPipe.enableTexture();
    blurPipe.setRenderPass(bloomRenderPass);
    blurPipe.create(pipeInfo);

    // Custom descriptor set layout for final bloom (2 textures)
    VkDescriptorSetLayout bloomFinalLayout{};
    {
        VkDescriptorSetLayoutBinding b0{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutBinding b1{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutBinding bindings[] = {b0, b1};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, bindings};
        vkCreateDescriptorSetLayout(inst.device, &li, nullptr, &bloomFinalLayout);
    }

    VulkanGraphicsPipeline bloomFinalPipe(chain, vertShaderCode, bloomFinalFragCode);
    bloomFinalPipe.enableTexture(); // Activa el uso de descriptores en el pipeline
    bloomFinalPipe.setDescriptorSetLayout(bloomFinalLayout);
    bloomFinalPipe.create(pipeInfo);

    bool useBloom = false;
    glfwSetWindowUserPointer(window, &useBloom);
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_B && action == GLFW_PRESS) {
            bool* bloom = (bool*)glfwGetWindowUserPointer(window);
            *bloom = !(*bloom);
            std::cout << "Post-processing: " << (*bloom ? "Bloom" : "Contrast") << std::endl;
        }
    });

    struct LightData { float position[4]; float color[4]; };
    struct GlobalUniform3D { float ProjView[16]; float camera[4]; LightData lights[3]; };
    struct MeshData { std::vector<float> vertices; std::vector<uint32_t> indices; };
    auto createBoxMesh = [](float w, float h, float d) -> MeshData {
        MeshData mesh;
        mesh.vertices = {
            -w,-h, d,  0,0, 1,  0,0,   w,-h, d,  0,0, 1,  1,0,   w, h, d,  0,0, 1,  1,1,  -w, h, d,  0,0, 1,  0,1,
             w,-h, d,  1,0,0,  0,0,   w,-h,-d,  1,0,0,  1,0,   w, h,-d,  1,0,0,  1,1,   w, h, d,  1,0,0,  0,1,
             w, h,-d,  0,0,-1, 1,1,  -w, h,-d,  0,0,-1, 0,1,  -w,-h,-d, 0,0,-1, 0,0,   w,-h,-d, 0,0,-1, 1,0,
            -w,-h,-d, -1,0,0, 0,0,  -w,-h, d, -1,0,0, 1,0,  -w, h, d, -1,0,0, 1,1,  -w, h,-d, -1,0,0, 0,1,
            -w, h,-d, 0,1,0, 0,1,   w, h,-d, 0,1,0, 1,1,   w, h, d, 0,1,0, 1,0,  -w, h, d, 0,1,0, 0,0,
            -w,-h, d, 0,-1,0, 0,0,   w,-h, d, 0,-1,0, 1,0,   w,-h,-d, 0,-1,0, 1,1,  -w,-h,-d, 0,-1,0, 0,1,
        };
        mesh.indices = {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9,10, 8,10,11,
           12,13,14,12,14,15,
           16,17,18,16,18,19,
           20,21,22,20,22,23
        };
        return mesh;
    };

    std::vector<char> cubeVert = readFile("cube.vert.spv");
    std::vector<char> cubeFrag = readFile("cube.frag.spv");
    VulkanGraphicsPipeline scenePipe(chain, cubeVert, cubeFrag);
    scenePipe.enableTexture();
    scenePipe.enableUniformBuffer();
    scenePipe.setUniformBufferBindingCount(2);
    scenePipe.setDescriptorSetLayout(descriptorSetLayout);
    scenePipe.enableDepthTest();
    VkVertexInputBindingDescription sceneBinding{};
    sceneBinding.binding = 0;
    sceneBinding.stride = sizeof(float) * 8;
    sceneBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription sceneAttrs[3];
    sceneAttrs[0].location = 0; sceneAttrs[0].binding = 0; sceneAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; sceneAttrs[0].offset = 0;
    sceneAttrs[1].location = 1; sceneAttrs[1].binding = 0; sceneAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; sceneAttrs[1].offset = sizeof(float)*3;
    sceneAttrs[2].location = 2; sceneAttrs[2].binding = 0; sceneAttrs[2].format = VK_FORMAT_R32G32_SFLOAT;  sceneAttrs[2].offset = sizeof(float)*6;
    VkPipelineVertexInputStateCreateInfo sceneVertexInput{};
    sceneVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    sceneVertexInput.vertexBindingDescriptionCount = 1;
    sceneVertexInput.pVertexBindingDescriptions = &sceneBinding;
    sceneVertexInput.vertexAttributeDescriptionCount = 3;
    sceneVertexInput.pVertexAttributeDescriptions = sceneAttrs;
    VkPipelineInputAssemblyStateCreateInfo sceneInput{};
    sceneInput.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    sceneInput.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    sceneInput.primitiveRestartEnable = VK_FALSE;
    VkPipelineViewportStateCreateInfo sceneViewportState{};
    sceneViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    sceneViewportState.viewportCount = 1;
    sceneViewportState.scissorCount = 1;
    VkGraphicsPipelineCreateInfo scenePipeInfo{};
    scenePipeInfo.pVertexInputState = &sceneVertexInput;
    scenePipeInfo.pInputAssemblyState = &sceneInput;
    scenePipeInfo.pViewportState = &sceneViewportState;
    scenePipe.setRenderPass(offRenderPass);
    scenePipe.create(scenePipeInfo);

    GlobalUniform3D sceneG{};
    VulkanBuffer sceneGUBO(inst);
    sceneGUBO.create(&sceneG, sizeof(sceneG), true);
    ModelUniform sceneM{};
    VulkanBuffer sceneMUBO(inst);
    sceneMUBO.create(&sceneM, sizeof(sceneM), true);
    scenePipe.createDescriptors(sceneGUBO, &srcTexture);
    {
        VkDescriptorBufferInfo modelInfo{};
        modelInfo.buffer = sceneMUBO.buffer;
        modelInfo.offset = 0;
        modelInfo.range = sceneMUBO.size;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = scenePipe.descriptorSets[i];
            write.dstBinding = 2;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &modelInfo;
            vkUpdateDescriptorSets(inst.device, 1, &write, 0, nullptr);
        }
    }

    auto cubeMesh = createBoxMesh(2.0f, 2.0f, 2.0f);
    VulkanBuffer sceneVBuf(inst);
    sceneVBuf.create(cubeMesh.vertices.data(), static_cast<int>(cubeMesh.vertices.size() * sizeof(float)), false);
    VulkanBuffer sceneIBuf(inst);
    sceneIBuf.createIndex(cubeMesh.indices.data(), static_cast<int>(cubeMesh.indices.size() * sizeof(uint32_t)));
    int sceneIndexCount = static_cast<int>(cubeMesh.indices.size());

    VulkanTexture offAsTexture;
    offAsTexture.imageView = offView;
    offAsTexture.sampler = offSampler;
    contrastPipe.createTextureDescriptor(offAsTexture);

    // Descriptor sets for bloom passes
    brightnessPipe.createTextureDescriptor(offAsTexture);

    VulkanTexture bloom1AsTex, bloom2AsTex;
    bloom1AsTex.imageView = bloomView1; bloom1AsTex.sampler = offSampler;
    bloom2AsTex.imageView = bloomView2; bloom2AsTex.sampler = offSampler;
    
    // Create a descriptor pool for custom bloom sets
    VkDescriptorPool bloomPool{};
    {
        VkDescriptorPoolSize poolSizes[1];
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}; // Enough for all passes
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 10, 1, poolSizes};
        vkCreateDescriptorPool(inst.device, &poolInfo, nullptr, &bloomPool);
    }

    // blurPipe will use two different descriptor sets for its two passes
    VkDescriptorSet blurDS1[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet blurDS2[MAX_FRAMES_IN_FLIGHT];
    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = blurPipe.descriptorSetLayout;
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, bloomPool, MAX_FRAMES_IN_FLIGHT, layouts};
        vkAllocateDescriptorSets(inst.device, &ai, blurDS1);
        vkAllocateDescriptorSets(inst.device, &ai, blurDS2);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo info1{offSampler, bloomView1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo info2{offSampler, bloomView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w1{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, blurDS1[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &info1};
            VkWriteDescriptorSet w2{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, blurDS2[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &info2};
            vkUpdateDescriptorSets(inst.device, 1, &w1, 0, nullptr);
            vkUpdateDescriptorSets(inst.device, 1, &w2, 0, nullptr);
        }
    }

    // Bloom final needs two textures: scene and bloom
    VkDescriptorSet bloomFinalDS[MAX_FRAMES_IN_FLIGHT];
    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = bloomFinalLayout;
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, bloomPool, MAX_FRAMES_IN_FLIGHT, layouts};
        vkAllocateDescriptorSets(inst.device, &allocInfo, bloomFinalDS);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo sceneInfo{offSampler, offView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo bloomInfo{offSampler, bloomView1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[2];
            w[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, bloomFinalDS[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, nullptr, nullptr};
            w[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, bloomFinalDS[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bloomInfo, nullptr, nullptr};
            vkUpdateDescriptorSets(inst.device, 2, w, 0, nullptr);
        }
    }

    VulkanRenderer renderer(chain);
    renderer.initialize(inst);

    VulkanBuffer tmp(inst);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        VkCommandBuffer cmdOff = tmp.beginSingleTimeCommands();
        VkClearValue offClears[2];
        offClears[0].color = {{0.3f, 0.3f, 0.3f, 1.0f}};
        offClears[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo offBegin{};
        offBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        offBegin.renderPass = offRenderPass;
        offBegin.framebuffer = offFramebuffer;
        offBegin.renderArea.offset = {0, 0};
        offBegin.renderArea.extent = chain.swapChainExtent;
        offBegin.clearValueCount = 2;
        offBegin.pClearValues = offClears;
        vkCmdBeginRenderPass(cmdOff, &offBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipe.graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) chain.swapChainExtent.width;
        viewport.height = (float) chain.swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdOff, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = chain.swapChainExtent;
        vkCmdSetScissor(cmdOff, 0, 1, &scissor);

        VkBuffer vb[] = { sceneVBuf.buffer };
        VkDeviceSize voff[] = { 0 };
        vkCmdBindVertexBuffers(cmdOff, 0, 1, vb, voff);
        vkCmdBindIndexBuffer(cmdOff, sceneIBuf.buffer, 0, VK_INDEX_TYPE_UINT32);
        {
            Mat4 proj = Mat4::perspective(45.0f * (float)M_PI / 180.0f, (float)chain.swapChainExtent.width / (float)chain.swapChainExtent.height, 0.1f, 100.0f);
            Vec3 eye(6.0f, 0.0f, 8.0f);
            Vec3 center(0.0f, 0.0f, 0.0f);
            Vec3 up(0.0f, 1.0f, 0.0f);
            Mat4 view = Mat4::lookAt(eye, center, up);
            Mat4 pv = proj * view;
            std::memcpy(sceneG.ProjView, pv.data(), sizeof(sceneG.ProjView));
            sceneG.camera[0] = eye.x; sceneG.camera[1] = eye.y; sceneG.camera[2] = eye.z; sceneG.camera[3] = 0.0f;
            sceneG.lights[0] = {{4,4,4,0}, {1,1,1,0}};
            sceneG.lights[1] = {{-4,4,4,0}, {1,0,0,0}};
            sceneG.lights[2] = {{0,4,-4,0}, {0,0,1,0}};
            sceneGUBO.update(&sceneG, sizeof(sceneG));

            float angle = (float)glfwGetTime();
            Mat4 model = Mat4::rotate(Mat4::identity(), angle, Vec3(0.0f, 1.0f, 0.0f));
            std::memcpy(sceneM.model, model.data(), sizeof(sceneM.model));
            sceneMUBO.update(&sceneM, sizeof(sceneM));
        }
        VkDescriptorSet offSet = scenePipe.descriptorSets[0];
        vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipe.pipelineLayout, 0, 1, &offSet, 0, nullptr);
        vkCmdDrawIndexed(cmdOff, sceneIndexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmdOff);

        if (useBloom) {
            // Bind full-screen quad vertex buffer for post-processing passes
            VkBuffer vbufs[] = { vbuf.buffer };
            VkDeviceSize voffs[] = { 0 };
            vkCmdBindVertexBuffers(cmdOff, 0, 1, vbufs, voffs);

            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, bloomRenderPass, bloomFB1, {{0,0}, chain.swapChainExtent}, 1, &clearColor};
            
            // 1. Brightness extraction
            vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, brightnessPipe.graphicsPipeline);
            vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, brightnessPipe.pipelineLayout, 0, 1, &brightnessPipe.descriptorSets[0], 0, nullptr);
            vkCmdDraw(cmdOff, 6, 1, 0, 0);
            vkCmdEndRenderPass(cmdOff);

            // 2. Horizontal Blur
            rpBegin.framebuffer = bloomFB2;
            vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.graphicsPipeline);
            float horizontalDir[2] = {1.0f, 0.0f};
            vkCmdPushConstants(cmdOff, blurPipe.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, horizontalDir);
            vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.pipelineLayout, 0, 1, &blurDS1[0], 0, nullptr);
            vkCmdDraw(cmdOff, 6, 1, 0, 0);
            vkCmdEndRenderPass(cmdOff);

            // 3. Vertical Blur
            rpBegin.framebuffer = bloomFB1;
            vkCmdBeginRenderPass(cmdOff, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.graphicsPipeline);
            float verticalDir[2] = {0.0f, 1.0f};
            vkCmdPushConstants(cmdOff, blurPipe.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, verticalDir);
            vkCmdBindDescriptorSets(cmdOff, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.pipelineLayout, 0, 1, &blurDS2[0], 0, nullptr);
            vkCmdDraw(cmdOff, 6, 1, 0, 0);
            vkCmdEndRenderPass(cmdOff);
        }

        tmp.endSingleTimeCommands(cmdOff);

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VulkanGraphicsPipeline& currentPipe = useBloom ? bloomFinalPipe : contrastPipe;
        VkCommandBuffer cmd = renderer.begin(currentPipe, &clearColor, 1);
        
        VkViewport viewport2{};
        viewport2.x = 0.0f;
        viewport2.y = 0.0f;
        viewport2.width = (float) chain.swapChainExtent.width;
        viewport2.height = (float) chain.swapChainExtent.height;
        viewport2.minDepth = 0.0f;
        viewport2.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport2);
        VkRect2D scissor2{};
        scissor2.offset = {0, 0};
        scissor2.extent = chain.swapChainExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor2);
        VkBuffer buffers2[] = { vbuf.buffer };
        VkDeviceSize offsets2[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, buffers2, offsets2);

        if (useBloom) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomFinalPipe.pipelineLayout, 0, 1, &bloomFinalDS[renderer.currentFrame], 0, nullptr);
        } else {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, contrastPipe.pipelineLayout, 0, 1, &contrastPipe.descriptorSets[renderer.currentFrame], 0, nullptr);
        }
        vkCmdDraw(cmd, 6, 1, 0, 0);
        
        renderer.end();
    }

    vkDeviceWaitIdle(inst.device);
    srcTexture.destroy();
    tmp.destroy();
    renderer.destroy();
    sceneMUBO.destroy();
    sceneGUBO.destroy();
    sceneIBuf.destroy();
    sceneVBuf.destroy();
    vbuf.destroy();
    scenePipe.destroy();
    contrastPipe.destroy();
    brightnessPipe.destroy();
    blurPipe.destroy();
    bloomFinalPipe.destroy();
    vkDestroyDescriptorSetLayout(inst.device, bloomFinalLayout, nullptr);
    vkDestroyFramebuffer(inst.device, bloomFB1, nullptr);
    vkDestroyFramebuffer(inst.device, bloomFB2, nullptr);
    vkDestroyRenderPass(inst.device, bloomRenderPass, nullptr);
    vkDestroyImageView(inst.device, bloomView1, nullptr);
    vkDestroyImageView(inst.device, bloomView2, nullptr);
    vkDestroyImage(inst.device, bloomImage1, nullptr);
    vkDestroyImage(inst.device, bloomImage2, nullptr);
    vkFreeMemory(inst.device, bloomMemory1, nullptr);
    vkFreeMemory(inst.device, bloomMemory2, nullptr);
    vkDestroyDescriptorPool(inst.device, bloomPool, nullptr);
    vkDestroyDescriptorSetLayout(inst.device, descriptorSetLayout, nullptr);
    vkDestroySampler(inst.device, offSampler, nullptr);
    vkDestroyImageView(inst.device, offView, nullptr);
    vkDestroyImageView(inst.device, offDepthView, nullptr);
    vkDestroyFramebuffer(inst.device, offFramebuffer, nullptr);
    vkDestroyRenderPass(inst.device, offRenderPass, nullptr);
    vkDestroyImage(inst.device, offImage, nullptr);
    vkFreeMemory(inst.device, offMemory, nullptr);
    vkDestroyImage(inst.device, offDepthImage, nullptr);
    vkFreeMemory(inst.device, offDepthMemory, nullptr);
    chain.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

