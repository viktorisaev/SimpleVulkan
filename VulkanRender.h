#pragma once

#include "vulkan/vulkan.h"

#include "VulkanBase/VulkanDevice.h"
#include "VulkanBase/VulkanSwapChain.h"

class VulkanRender
{
public:
    bool Init(HINSTANCE instance, HWND hwnd);

    void Begin();
    void End();

    void Finalize();

private:
    void initVulkan();
    VkResult createInstance();
    void createSurface(HINSTANCE hInstance, HWND hwnd);

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
    
    std::vector<VkLayerSettingEXT> m_enabledLayerSettings;    // @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) 
    
    std::vector<const char*> m_enabledDeviceExtensions;   // @brief Set of device extensions to be enabled for this example (must be set in the derived constructor)
    std::vector<const char*> m_enabledInstanceExtensions; // @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor)
    std::vector<std::string> m_supportedInstanceExtensions;

    uint32_t m_apiVersion = VK_API_VERSION_1_0;

    VulkanSwapChain m_swapChain;
    vks::VulkanDevice* vulkanDevice{};

};
