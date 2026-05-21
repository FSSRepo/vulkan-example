#ifndef __VULKAN_GRAPHICS_PIPELINE_H__
#define __VULKAN_GRAPHICS_PIPELINE_H__

#include "vkCommon.h"
#include "VulkanSwapchain.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"

class VulkanGraphicsPipeline {
    private:
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VulkanSwapchain swapChain;

    bool useTexture = false;
    uint32_t uniformBindingCount = 0;
    bool depthOverride = false;
    bool depthDisable = false;
    bool ownsDescriptorSetLayout = false;
    VkDescriptorPool descriptorPool{};

    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool alphaBlendEnabled = false;

    public:
    VkDescriptorSetLayout descriptorSetLayout{};
    std::vector<VkDescriptorSet> descriptorSets;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;

    VulkanGraphicsPipeline(VulkanSwapchain swap, const std::vector<char>& vertex, const std::vector<char>& fragment);

    void create();
    void setVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                        const std::vector<VkVertexInputAttributeDescription>& attributes);
    void setTopology(VkPrimitiveTopology topology);
    void enableAlphaBlending();
    void enableTexture();
    void enableUniformBuffer();
    void setUniformBufferBindingCount(uint32_t count);
    void enableDepthTest();
    void disableDepthTest();
    void createTextureDescriptor(VulkanTexture &texture);
    void createDescriptors(VulkanBuffer &ubo, VulkanTexture *texture);
    void setDescriptorSetLayout(VkDescriptorSetLayout layout);
    void setRenderPass(VkRenderPass renderPass);
    void bind(VkCommandBuffer commandBuffer);
    void bindDescriptorSets(VkCommandBuffer commandBuffer, int frameIndex, VkPipelineBindPoint bindPoint);
    void destroy();
};

#endif // __VULKAN_GRAPHICS_PIPELINE_H__
