#include "VulkanRender.h"

#include "VulkanBase/VulkanDebug.h"



bool VulkanRender::Init(HINSTANCE hInstance, HWND hwnd)
{
	initVulkan();
    createSurface(hInstance, hwnd);

    return true;
}

void VulkanRender::Begin()
{
}

void VulkanRender::End()
{
}

void VulkanRender::Finalize()
{
	// Clean up used Vulkan resources
	// Note: Inherited destructor cleans up resources stored in base class
	if (vulkDevice)
	{
//		vkDestroyPipeline(vulkDevice, pipeline, nullptr);
//		vkDestroyPipelineLayout(vulkDevice, pipelineLayout, nullptr);
//		vkDestroyDescriptorSetLayout(vulkDevice, descriptorSetLayout, nullptr);
//		vkDestroyBuffer(vulkDevice, vertices.buffer, nullptr);
//		vkFreeMemory(vulkDevice, vertices.memory, nullptr);
//		vkDestroyBuffer(vulkDevice, indices.buffer, nullptr);
//		vkFreeMemory(vulkDevice, indices.memory, nullptr);
//		vkDestroyCommandPool(vulkDevice, commandPool, nullptr);
		//for (size_t i = 0; i < presentCompleteSemaphores.size(); i++) {
		//	vkDestroySemaphore(vulkDevice, presentCompleteSemaphores[i], nullptr);
		//}
		//for (size_t i = 0; i < renderCompleteSemaphores.size(); i++) {
		//	vkDestroySemaphore(vulkDevice, renderCompleteSemaphores[i], nullptr);
		//}
		//for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
		//	vkDestroyFence(vulkDevice, waitFences[i], nullptr);
		//	vkDestroyBuffer(vulkDevice, uniformBuffers[i].buffer, nullptr);
		//	vkFreeMemory(vulkDevice, uniformBuffers[i].memory, nullptr);
		//}
	}
}


#pragma region Internal

void VulkanRender::initVulkan()
{
	// Create the instance
	VK_CHECK_RESULT(createInstance());

	// If requested, we enable the default validation layers for debugging
//	if (settings.validation)
	{
		vks::debug::setupDebugging(vulkInstance);
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vulkInstance, &gpuCount, nullptr));
	if (gpuCount == 0)
	{
		throw std::runtime_error("No device with Vulkan support found");
	}
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vulkInstance, &gpuCount, physicalDevices.data()));

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 0;

	{
		std::cout << "Available Vulkan devices" << "\n";
		for (uint32_t i = 0; i < gpuCount; i++) {
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
			std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
			std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
			std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
		}
	}

	vulkPhysicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(vulkPhysicalDevice, &vulkDeviceProperties);
	vkGetPhysicalDeviceFeatures(vulkPhysicalDevice, &vulkDeviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(vulkPhysicalDevice, &vulkDeviceMemoryProperties);

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
//	getEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	vulkanDevice = new vks::VulkanDevice(vulkPhysicalDevice);

	// Derived examples can enable extensions based on the list of supported extensions read from the physical device
//	getEnabledExtensions();

	VK_CHECK_RESULT(vulkanDevice->createLogicalDevice(vulkEnabledFeatures, m_enabledDeviceExtensions, vulkDeviceCreatepNextChain));
	vulkDevice = vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(vulkDevice, vulkanDevice->queueFamilyIndices.graphics, 0, &vulkQueue);

	// Find a suitable depth and/or stencil format
	VkBool32 validFormat{ false };
	// Samples that make use of stencil will require a depth + stencil format, so we select from a different list
	//if (requiresStencil)
	//{
	//	validFormat = vks::tools::getSupportedDepthStencilFormat(physicalDevice, &depthFormat);
	//}
	//else
	{
		validFormat = vks::tools::getSupportedDepthFormat(vulkPhysicalDevice, &vulkDepthFormat);
	}
	assert(validFormat);

	m_swapChain.setContext(vulkInstance, vulkPhysicalDevice, vulkDevice);
}




VkResult VulkanRender::createInstance()
{
	std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Enable surface extensions depending on os
#if defined(_WIN32)
	instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
	instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	instanceExtensions.push_back(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	instanceExtensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	instanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
	instanceExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_SCREEN_QNX)
	instanceExtensions.push_back(VK_QNX_SCREEN_SURFACE_EXTENSION_NAME);
#endif

	// Get extensions supported by the instance and store for later use
	uint32_t extCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (VkExtensionProperties& extension : extensions)
			{
				m_supportedInstanceExtensions.push_back(extension.extensionName);
			}
		}
	}

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
	// SRS - When running on iOS/macOS with MoltenVK, enable VK_KHR_get_physical_device_properties2 if not already enabled by the example (required by VK_KHR_portability_subset)
	if (std::find(enabledInstanceExtensions.begin(), enabledInstanceExtensions.end(), VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == enabledInstanceExtensions.end())
	{
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}
#endif

	// Enabled requested instance extensions
	if (!m_enabledInstanceExtensions.empty())
	{
		for (const char* enabledExtension : m_enabledInstanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(m_supportedInstanceExtensions.begin(), m_supportedInstanceExtensions.end(), enabledExtension) == m_supportedInstanceExtensions.end())
			{
				std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
			}
			instanceExtensions.push_back(enabledExtension);
		}
	}

	// Shaders generated by Slang require a certain SPIR-V environment that can't be satisfied by Vulkan 1.0, so we need to expliclity up that to at least 1.1 and enable some required extensions
//	if (shaderDir == "slang")
	{
		if (m_apiVersion < VK_API_VERSION_1_1)
		{
			m_apiVersion = VK_API_VERSION_1_1;
		}
		m_enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		m_enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		m_enabledDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
	}

	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "VI App name",
		.pEngineName = "VI Engine name",
		.apiVersion = m_apiVersion
	};

	VkInstanceCreateInfo instanceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo
	};

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
//	if (settings.validation)
	{
		vks::debug::setupDebugingMessengerCreateInfo(debugUtilsMessengerCI);
		debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
		instanceCreateInfo.pNext = &debugUtilsMessengerCI;
	}

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT)) && defined(VK_KHR_portability_enumeration)
	// SRS - When running on iOS/macOS with MoltenVK and VK_KHR_portability_enumeration is defined and supported by the instance, enable the extension and the flag
	if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) != supportedInstanceExtensions.end())
	{
		instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
#endif

	// Enable the debug utils extension if available (e.g. when debugging tools are present)
	if (/*settings.validation || */std::find(m_supportedInstanceExtensions.begin(), m_supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != m_supportedInstanceExtensions.end())
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if (!instanceExtensions.empty())
	{
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}

	// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
	// Note that on Android this layer requires at least NDK r20
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
//	if (settings.validation)
	{
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		for (VkLayerProperties& layer : instanceLayerProperties) {
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		}
		else {
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}

	// If layer settings are defined, then activate the sample's required layer settings during instance creation.
	// Layer settings are typically used to activate specific features of a layer, such as the Validation Layer's
	// printf feature, or to configure specific capabilities of drivers such as MoltenVK on macOS and/or iOS.
	VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo{ .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT };
	if (m_enabledLayerSettings.size() > 0)
	{
		layerSettingsCreateInfo.settingCount = static_cast<uint32_t>(m_enabledLayerSettings.size());
		layerSettingsCreateInfo.pSettings = m_enabledLayerSettings.data();
		layerSettingsCreateInfo.pNext = instanceCreateInfo.pNext;
		instanceCreateInfo.pNext = &layerSettingsCreateInfo;
	}

	VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &vulkInstance);

	// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
	if (std::find(m_supportedInstanceExtensions.begin(), m_supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != m_supportedInstanceExtensions.end())
	{
		vks::debugutils::setup(vulkInstance);
	}

	return result;
}


void VulkanRender::createSurface(HINSTANCE hInstance, HWND hwnd)
{
#if defined(_WIN32)
	m_swapChain.initSurface(hInstance, hwnd);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	swapChain.initSurface(androidApp->window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
	swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	swapChain.initSurface(metalLayer);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	swapChain.initSurface(connection, window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
	swapChain.initSurface(width, height);
#elif defined(VK_USE_PLATFORM_SCREEN_QNX)
	swapChain.initSurface(screen_context, screen_window);
#endif
}

#pragma endregion Internal