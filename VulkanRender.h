#pragma once

#include <vector>
#include <array>

#include "vulkan/vulkan.h"

#include "VulkanBase/VulkanDevice.h"
#include "VulkanBase/VulkanSwapChain.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>


// We want to keep GPU and CPU busy. To do that we may start building a new command buffer while the previous one is still being executed
// This number defines how many frames may be worked on simultaneously at once
// Increasing this number may improve performance but will also introduce additional latency
constexpr auto MAX_CONCURRENT_FRAMES = 2;

/** @brief Default depth stencil attachment used by the default render pass */
struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} depthStencil{};


// Uniform buffer block object
struct UniformBuffer {
    VkDeviceMemory memory{ VK_NULL_HANDLE };
    VkBuffer buffer{ VK_NULL_HANDLE };
    // The descriptor set stores the resources bound to the binding points in a shader
    // It connects the binding points of the different shaders with the buffers and images used for those bindings
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    // We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
    uint8_t* mapped{ nullptr };
};

struct Vertex {
    float position[3];
    float normal[3];
};



class VulkanRender
{
public:
    bool Init(HINSTANCE instance, HWND hwnd);

    void RenderFrame();

    void Finalize();

private:
    void initVulkan();
    VkResult createInstance();
    void createSurface(HINSTANCE hInstance, HWND hwnd);
    void createSwapChain();
    void createSynchronizationPrimitives();
    void setupDepthStencil();
    void createCommandBuffers();
    void setupRenderPass();
    void setupFrameBuffer();
    void createUniformBuffers();
    void createPipelines();

    VkShaderModule loadSPIRVShader(const std::string& filename);
    uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);
    void updateViewMatrix();

private:
    VkInstance vulkInstance{ VK_NULL_HANDLE };
    VkDevice vulkDevice{ VK_NULL_HANDLE };
    VkPhysicalDevice vulkPhysicalDevice{ VK_NULL_HANDLE };  // Physical device (GPU) that Vulkan will use
    VkPhysicalDeviceProperties vulkDeviceProperties{};  // Stores physical device properties (for e.g. checking device limits)
    VkPhysicalDeviceFeatures vulkDeviceFeatures{};  // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
    VkPhysicalDeviceMemoryProperties vulkDeviceMemoryProperties{};  // Stores all available memory (type) properties for the physical device
    VkPhysicalDeviceFeatures vulkEnabledFeatures{}; // @brief Set of physical device features to be enabled for this example (must be set in the derived constructor)
    void* vulkDeviceCreatepNextChain = nullptr;     // @brief Optional pNext structure for passing extension structures to device creation
    VkQueue vulkQueue{ VK_NULL_HANDLE };    // Handle to the device graphics queue that command buffers are submitted to
    VkFormat vulkDepthFormat{ VK_FORMAT_UNDEFINED };    // Depth buffer format (selected during Vulkan initialization)
    VkCommandPool vulkCommandPool{ VK_NULL_HANDLE };
    std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> vulkCommandBuffers{};
    std::array<VkFence, MAX_CONCURRENT_FRAMES> vulkWaitFences{};
    std::vector<VkFramebuffer>vulkFrameBuffers;     // List of available frame buffers (same as number of swap chain images)
    VkRenderPass vulkRenderPass{ VK_NULL_HANDLE };  // Global render pass for frame buffer writes
    // The pipeline layout is used by a pipeline to access the descriptor sets
    // It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
    // A pipeline layout can be shared among multiple pipelines as long as their interfaces match
    VkPipelineLayout vulkPipelineLayout{ VK_NULL_HANDLE };
    // The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
    // Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
    VkDescriptorSetLayout vulkDescriptorSetLayout{ VK_NULL_HANDLE };
    VkPipelineCache vulkPipelineCache{ VK_NULL_HANDLE };    // Pipeline cache object
    // Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
    // While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
    // So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
    // Even though this adds a new dimension of planning ahead, it's a great opportunity for performance optimizations by the driver
    VkPipeline vulkPipeline{ VK_NULL_HANDLE };



    std::vector<VkLayerSettingEXT> m_enabledLayerSettings;    // @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) 

    // Semaphores are used to coordinate operations within the graphics queue and ensure correct command ordering
    std::vector<VkSemaphore> m_presentCompleteSemaphores{};
    std::vector<VkSemaphore> m_renderCompleteSemaphores{};

    std::vector<const char*> m_enabledDeviceExtensions;   // @brief Set of device extensions to be enabled for this example (must be set in the derived constructor)
    std::vector<const char*> m_enabledInstanceExtensions; // @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor)
    std::vector<std::string> m_supportedInstanceExtensions;

    uint32_t m_apiVersion = VK_API_VERSION_1_0;

    VulkanSwapChain m_swapChain;
    vks::VulkanDevice* vulkanDevice{};

    bool prepared = false;
    bool resized = false;
    uint32_t width = 1280;
    uint32_t height = 720;

    uint32_t m_currentFrame{ 0 }; // To select the correct sync and command objects, we need to keep track of the current frame

    std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniformBuffers;    // We use one UBO per frame, so we can have a frame overlap and make sure that uniforms aren't updated while still in use

    glm::mat4 m_viewMatrix;
};
