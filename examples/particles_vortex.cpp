#include "vkApp.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include "utils.h"

#include "graphics_math.h"

#define WIDTH 1280
#define HEIGHT 720
#define PARTICLE_COUNT 65536
#define MAX_FRAMES_IN_FLIGHT 2

struct Particle {
    Vec4 pos;
    Vec4 vel;
    Vec4 color;
};

struct SimParams {
    float dt;
    float time;
};

struct CameraUBO {
    Mat4 viewProj;
};

class Camera {
public:
    Vec3 position;
    Vec3 up;
    Vec3 direction;
    float fov;
    float aspect;
    float nearClip;
    float farClip;
    float yaw;
    float pitch;
    Camera() {
        position = Vec3(0.0f, 2.0f, 8.0f);
        up = Vec3(0.0f, 1.0f, 0.0f);
        direction = -position;
        fov = 45.f * pi() / 180.0f;
        aspect = (float)WIDTH/(float)HEIGHT;
        nearClip = 0.1f;
        farClip = 100.0f;
        yaw = std::atan2(position.z, position.x);
        pitch = std::asin(position.y / length(position));
    }
    void orbit(float nx, float ny) {
        float sens = 1.0f;
        yaw += nx * sens;
        pitch += ny * sens;
        float limit = radians(89.0f);
        if (pitch > limit) pitch = limit;
        if (pitch < -limit) pitch = -limit;
        float radius = length(position);
        float cx = std::cos(pitch);
        position.x = radius * cx * std::cos(yaw);
        position.y = radius * std::sin(pitch);
        position.z = radius * cx * std::sin(yaw);
        direction = -position;
    }
    Mat4 getProjViewMatrix() const {
        Mat4 proj = Mat4::perspective(fov, aspect, nearClip, farClip);
        Mat4 view = Mat4::lookAt(position, position + direction, up);
        return proj * view;
    }
};

class ParticleSystem {
public:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkBuffer particleBuffers[2];
    VkDeviceMemory particleBufferMemories[2];
    VkBuffer simParamsBuffer;
    VkDeviceMemory simParamsMemory;
    void* simParamsMapped = nullptr;
    VkBuffer cameraBuffer;
    VkDeviceMemory cameraMemory;
    void* cameraMapped = nullptr;

    VkShaderModule computeShaderModule = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSets[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSets[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSetLayout graphicsSetLayout = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

    uint32_t currentFrame = 0;
    float currentTime = 0.0f;

    void init(VulkanInstance& inst, VulkanSwapchain& chain,
              const std::vector<char>& compCode,
              const std::vector<char>& vertCode,
              const std::vector<char>& fragCode,
              VkDescriptorSetLayout graphicsLayout) {
        device = inst.device;
        physicalDevice = inst.physicalDevice;
        graphicsQueue = inst.graphicsQueue;
        presentQueue = inst.presentQueue;
        graphicsSetLayout = graphicsLayout;

        createCommandPool();
        createBuffers();
        createComputePipeline(compCode);
        createGraphicsDescriptors();
        createCommandBuffers();

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects!");
            }
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, props);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }
        vkBindBufferMemory(device, buffer, memory, 0);
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = 0;
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createBuffers() {
        VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

        std::vector<Particle> particles(PARTICLE_COUNT);
        for (size_t i = 0; i < PARTICLE_COUNT; i++) {
            float vx = ((float)rand() / RAND_MAX - 0.5f) * 3.0f;
            float vy = ((float)rand() / RAND_MAX * 1.5f + 0.5f) * 2.0f;
            float vz = ((float)rand() / RAND_MAX - 0.5f) * 3.0f;
            float life = 2.0f + ((float)rand() / RAND_MAX) * 2.0f;
            particles[i].pos = Vec4(0.0f, -1.0f, 0.0f, life);
            particles[i].vel = Vec4(vx, vy, vz, life);
            particles[i].color = Vec4(0.0f + ((float)rand()/RAND_MAX)*0.2f,
                                           0.6f + ((float)rand()/RAND_MAX)*0.4f,
                                           0.0f + ((float)rand()/RAND_MAX)*0.2f,
                                           1.0f);
        }

        for (int i = 0; i < 2; i++) {
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                particleBuffers[i], particleBufferMemories[i]);

            VkBuffer staging;
            VkDeviceMemory stagingMem;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                staging, stagingMem);
            void* data;
            vkMapMemory(device, stagingMem, 0, bufferSize, 0, &data);
            std::memcpy(data, particles.data(), (size_t)bufferSize);
            vkUnmapMemory(device, stagingMem);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = commandPool;
            allocInfo.commandBufferCount = 1;
            VkCommandBuffer cmd;
            vkAllocateCommandBuffers(device, &allocInfo, &cmd);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &beginInfo);
            VkBufferCopy copyRegion{};
            copyRegion.size = bufferSize;
            vkCmdCopyBuffer(cmd, staging, particleBuffers[i], 1, &copyRegion);
            vkEndCommandBuffer(cmd);
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);
            vkDestroyBuffer(device, staging, nullptr);
            vkFreeMemory(device, stagingMem, nullptr);
        }

        createBuffer(sizeof(SimParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            simParamsBuffer, simParamsMemory);
        vkMapMemory(device, simParamsMemory, 0, sizeof(SimParams), 0, &simParamsMapped);

        createBuffer(sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            cameraBuffer, cameraMemory);
        vkMapMemory(device, cameraMemory, 0, sizeof(CameraUBO), 0, &cameraMapped);
    }

    void createComputePipeline(const std::vector<char>& compCode) {
        computeShaderModule = createShaderModule(compCode);

        VkDescriptorSetLayoutBinding bindings[3];
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[2].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStage.module = computeShaderModule;
        shaderStage.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStage;
        pipelineInfo.layout = computePipelineLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        VkDescriptorPoolSize poolSizes[2];
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }

        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDescriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocInfo, computeDescriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate compute descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo infos[3];
            infos[0].buffer = particleBuffers[i % 2];
            infos[0].offset = 0;
            infos[0].range = VK_WHOLE_SIZE;

            infos[1].buffer = particleBuffers[(i + 1) % 2];
            infos[1].offset = 0;
            infos[1].range = VK_WHOLE_SIZE;

            infos[2].buffer = simParamsBuffer;
            infos[2].offset = 0;
            infos[2].range = sizeof(SimParams);

            VkWriteDescriptorSet writes[3];
            for (int b = 0; b < 3; b++) {
                writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[b].pNext = nullptr;
                writes[b].dstSet = computeDescriptorSets[i];
                writes[b].dstBinding = b;
                writes[b].dstArrayElement = 0;
                writes[b].descriptorType = (b == 2) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writes[b].descriptorCount = 1;
                writes[b].pImageInfo = nullptr;
                writes[b].pBufferInfo = &infos[b];
                writes[b].pTexelBufferView = nullptr;
            }
            vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
        }
    }

    void createGraphicsDescriptors() {
        VkDescriptorPoolSize poolSizes[2];
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &graphicsDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics descriptor pool!");
        }

        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, graphicsSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = graphicsDescriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocInfo, graphicsDescriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate graphics descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo infos[2];
            infos[0].buffer = particleBuffers[(i + 1) % 2];
            infos[0].offset = 0;
            infos[0].range = VK_WHOLE_SIZE;

            infos[1].buffer = cameraBuffer;
            infos[1].offset = 0;
            infos[1].range = sizeof(CameraUBO);

            VkWriteDescriptorSet writes[2];
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].pNext = nullptr;
            writes[0].dstSet = graphicsDescriptorSets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = nullptr;
            writes[0].pBufferInfo = &infos[0];
            writes[0].pTexelBufferView = nullptr;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].pNext = nullptr;
            writes[1].dstSet = graphicsDescriptorSets[i];
            writes[1].dstBinding = 2;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = nullptr;
            writes[1].pBufferInfo = &infos[1];
            writes[1].pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
        }
    }

    void createCommandBuffers() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(uint32_t frame, uint32_t imageIndex, VulkanSwapchain& chain,
                             VulkanGraphicsPipeline& gpipe, float dt, Camera& cam) {
        vkWaitForFences(device, 1, &inFlightFences[frame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[frame]);

        vkResetCommandBuffer(commandBuffers[frame], 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffers[frame], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        SimParams params{};
        params.dt = dt;
        params.time = currentTime;
        std::memcpy(simParamsMapped, &params, sizeof(params));

        CameraUBO camUbo{};
        camUbo.viewProj = cam.getProjViewMatrix();
        std::memcpy(cameraMapped, &camUbo, sizeof(camUbo));

        vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_COMPUTE,
                                computePipelineLayout, 0, 1, &computeDescriptorSets[frame], 0, nullptr);
        vkCmdDispatch(commandBuffers[frame], (PARTICLE_COUNT + 255) / 256, 1, 1);

        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = particleBuffers[(frame + 1) % 2];
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(commandBuffers[frame],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            0, nullptr,
            1, &barrier,
            0, nullptr);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = chain.renderPass;
        renderPassInfo.framebuffer = chain.swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = chain.swapChainExtent;

        VkClearValue clearColor = {{{0.02f, 0.02f, 0.05f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[frame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, gpipe.graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gpipe.pipelineLayout, 0, 1, &graphicsDescriptorSets[frame], 0, nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)chain.swapChainExtent.width;
        viewport.height = (float)chain.swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = chain.swapChainExtent;
        vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);

        vkCmdDraw(commandBuffers[frame], PARTICLE_COUNT, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[frame]);

        if (vkEndCommandBuffer(commandBuffers[frame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void render(VulkanSwapchain& chain, VulkanGraphicsPipeline& gpipe, float dt, Camera& cam) {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, chain.swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        recordCommandBuffer(currentFrame, imageIndex, chain, gpipe, dt, cam);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {chain.swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        currentTime += dt;
    }

    void destroy() {
        vkDeviceWaitIdle(device);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
        if (computeDescriptorPool) vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        if (computeDescriptorSetLayout) vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        if (computePipeline) vkDestroyPipeline(device, computePipeline, nullptr);
        if (computePipelineLayout) vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        if (computeShaderModule) vkDestroyShaderModule(device, computeShaderModule, nullptr);
        if (graphicsDescriptorPool) vkDestroyDescriptorPool(device, graphicsDescriptorPool, nullptr);

        if (simParamsMapped) vkUnmapMemory(device, simParamsMemory);
        if (cameraMapped) vkUnmapMemory(device, cameraMemory);

        for (int i = 0; i < 2; i++) {
            if (particleBuffers[i]) vkDestroyBuffer(device, particleBuffers[i], nullptr);
            if (particleBufferMemories[i]) vkFreeMemory(device, particleBufferMemories[i], nullptr);
        }
        if (simParamsBuffer) vkDestroyBuffer(device, simParamsBuffer, nullptr);
        if (simParamsMemory) vkFreeMemory(device, simParamsMemory, nullptr);
        if (cameraBuffer) vkDestroyBuffer(device, cameraBuffer, nullptr);
        if (cameraMemory) vkFreeMemory(device, cameraMemory, nullptr);
    }
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Particles - Vortex", nullptr, nullptr);

    bool debug_app = true;
    VulkanInstance inst(debug_app);
    inst.attach(window);
    inst.initializeDevice();

    VulkanSwapchain chain(inst);
    chain.initalize(WIDTH, HEIGHT, false);

    auto compShaderCode = readFile("vortex.comp.spv");
    auto vertShaderCode = readFile("particles.vert.spv");
    auto fragShaderCode = readFile("particles.frag.spv");

    VulkanGraphicsPipeline gpipe(chain, vertShaderCode, fragShaderCode);

    gpipe.setTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    gpipe.enableAlphaBlending();

    VkDescriptorSetLayoutBinding bindings[2];
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding = 2;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    VkDescriptorSetLayout graphicsDescriptorSetLayout;
    if (vkCreateDescriptorSetLayout(inst.device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout in main");
    }
    gpipe.setDescriptorSetLayout(graphicsDescriptorSetLayout);
    gpipe.setUniformBufferBindingCount(1);
    gpipe.create();

    ParticleSystem particles;
    particles.init(inst, chain, compShaderCode, vertShaderCode, fragShaderCode, graphicsDescriptorSetLayout);

    Camera camera;
    camera.position = Vec3(0.0f, 2.0f, 8.0f);
    camera.direction = -camera.position;
    camera.yaw = std::atan2(camera.position.z, camera.position.x);
    camera.pitch = std::asin(camera.position.y / length(camera.position));

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            static double lastX = x, lastY = y;
            double dx = x - lastX;
            double dy = y - lastY;
            camera.orbit(static_cast<float>(-dx * 0.005), static_cast<float>(-dy * 0.005));
            lastX = x;
            lastY = y;
        }

        particles.render(chain, gpipe, dt, camera);
    }

    vkDeviceWaitIdle(inst.device);
    particles.destroy();
    vkDestroyDescriptorSetLayout(inst.device, graphicsDescriptorSetLayout, nullptr);
    gpipe.destroy();
    chain.destroy();
    inst.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
