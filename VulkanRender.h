#pragma once

#include <vector>
#include <array>

#include "vulkan/vulkan.h"

#include "VulkanBase/VulkanDevice.h"
#include "VulkanBase/VulkanSwapChain.h"


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

    uint32_t currentFrame{ 0 }; // To select the correct sync and command objects, we need to keep track of the current frame

};
