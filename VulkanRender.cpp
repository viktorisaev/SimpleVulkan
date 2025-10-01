#include "VulkanRender.h"

#include "VulkanBase/VulkanDebug.h"


bool VulkanRender::Init(HINSTANCE hInstance, HWND hwnd)
{
	initVulkan();
    createSurface(hInstance, hwnd);

	createSwapChain();

	createSynchronizationPrimitives();
	createCommandBuffers();
	setupDepthStencil();

	createUniformBuffers();

	setupRenderPass();
	setupFrameBuffer();

	updateViewMatrix();	// set m_viewMatrix


	// TODO: remove it from here!
	prepared = true;

    return true;
}


// For simplicity we use the same uniform block layout as in the shader:
//
//	layout(set = 0, binding = 0) uniform UBO
//	{
//		mat4 projectionMatrix;
//		mat4 modelMatrix;
//		mat4 viewMatrix;
//	} ubo;
//
// This way we can just memcopy the ubo data to the ubo
// Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat4)
struct ShaderData {
	glm::mat4 projectionMatrix;
	glm::mat4 modelMatrix;
	glm::mat4 viewMatrix;
};


void VulkanRender::RenderFrame()
{
	if (!prepared)
	{
		return;
	}

	// Use a fence to wait until the command buffer has finished execution before using it again
	vkWaitForFences(vulkDevice, 1, &vulkWaitFences[m_currentFrame], VK_TRUE, UINT64_MAX);
	VK_CHECK_RESULT(vkResetFences(vulkDevice, 1, &vulkWaitFences[m_currentFrame]));

	// Get the next swap chain image from the implementation
	// Note that the implementation is free to return the images in any order, so we must use the acquire function and can't just cycle through the images/imageIndex on our own
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(vulkDevice, m_swapChain.swapChain, UINT64_MAX, m_presentCompleteSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
//TODO:		windowResize();
		return;
	}
	else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
	{
		throw "Could not acquire the next swap chain image!";
	}

	// Update the uniform buffer for the next frame
	ShaderData shaderData{};
	shaderData.projectionMatrix = glm::perspective(glm::radians(glm::pi<float>()/2.0f), float(width)/float(height), 0.1f, 100.0f); //camera.matrices.perspective;
	shaderData.viewMatrix = m_viewMatrix;
	shaderData.modelMatrix = glm::mat4(1.0f);

	// Copy the current matrices to the current frame's uniform buffer
	// Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
	memcpy(m_uniformBuffers[m_currentFrame].mapped, &shaderData, sizeof(ShaderData));

	// Build the command buffer
	// Unlike in OpenGL all rendering commands are recorded into command buffers that are then submitted to the queue
	// This allows to generate work upfront in a separate thread
	// For basic command buffers (like in this sample), recording is so fast that there is no need to offload this

	vkResetCommandBuffer(vulkCommandBuffers[m_currentFrame], 0);

	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Set clear values for all framebuffer attachments with loadOp set to clear
	// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
	VkClearValue clearValues[2]{};
	clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = vulkRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.framebuffer = vulkFrameBuffers[imageIndex];

	const VkCommandBuffer commandBuffer = vulkCommandBuffers[m_currentFrame];
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

	// Start the first sub pass specified in our default render pass setup by the base class
	// This will clear the color and depth attachment
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	// Update dynamic viewport state
	VkViewport viewport{};
	viewport.height = (float)height;
	viewport.width = (float)width;
	viewport.minDepth = (float)0.0f;
	viewport.maxDepth = (float)1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	// Update dynamic scissor state
	VkRect2D scissor{};
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


//TODO:
	//// Bind descriptor set for the current frame's uniform buffer, so the shader uses the data from that buffer for this draw
	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &uniformBuffers[m_currentFrame].descriptorSet, 0, nullptr);
	//// Bind the rendering pipeline
	//// The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
	//vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	//// Bind triangle vertex buffer (contains position and colors)
	//VkDeviceSize offsets[1]{ 0 };
	//vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
	//// Bind triangle index buffer
	//vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT16);
	//// Draw indexed triangle
	//vkCmdDrawIndexed(commandBuffer, indices.count, 1, 0, 0, 0);



	vkCmdEndRenderPass(commandBuffer);
	// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system
	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer to the graphics queue

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// The submit info structure specifies a command buffer queue submission batch
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStageMask;      // Pointer to the list of pipeline stages that the semaphore waits will occur at
	submitInfo.pCommandBuffers = &commandBuffer;		// Command buffers(s) to execute in this batch (submission)
	submitInfo.commandBufferCount = 1;                  // We submit a single command buffer

	// Semaphore to wait upon before the submitted command buffer starts executing
	submitInfo.pWaitSemaphores = &m_presentCompleteSemaphores[m_currentFrame];
	submitInfo.waitSemaphoreCount = 1;
	// Semaphore to be signaled when command buffers have completed
	submitInfo.pSignalSemaphores = &m_renderCompleteSemaphores[imageIndex];
	submitInfo.signalSemaphoreCount = 1;

	// Submit to the graphics queue passing a wait fence
	VK_CHECK_RESULT(vkQueueSubmit(vulkQueue, 1, &submitInfo, vulkWaitFences[m_currentFrame]));

	// Present the current frame buffer to the swap chain
	// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
	// This ensures that the image is not presented to the windowing system until all commands have been submitted

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderCompleteSemaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain.swapChain;
	presentInfo.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(vulkQueue, &presentInfo);

	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
	{
//TODO:		windowResize();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not present the image to the swap chain!");
	}

	// Select the next frame to render to, based on the max. no. of concurrent frames
	m_currentFrame = (m_currentFrame + 1) % MAX_CONCURRENT_FRAMES;
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
		vkDestroyCommandPool(vulkDevice, vulkCommandPool, nullptr);
		for (size_t i = 0; i < m_presentCompleteSemaphores.size(); i++)
		{
			vkDestroySemaphore(vulkDevice, m_presentCompleteSemaphores[i], nullptr);
		}
		for (size_t i = 0; i < m_renderCompleteSemaphores.size(); i++)
		{
			vkDestroySemaphore(vulkDevice, m_renderCompleteSemaphores[i], nullptr);
		}
		for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			vkDestroyFence(vulkDevice, vulkWaitFences[i], nullptr);
			//vkDestroyBuffer(vulkDevice, uniformBuffers[i].buffer, nullptr);
			//vkFreeMemory(vulkDevice, uniformBuffers[i].memory, nullptr);
		}

		m_swapChain.cleanup();
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



//void VulkanRender::nextFrame()
//{
//	auto tStart = std::chrono::high_resolution_clock::now();
//	render();
//	frameCounter++;
//	auto tEnd = std::chrono::high_resolution_clock::now();
//#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT)) && !defined(VK_EXAMPLE_XCODE_GENERATED)
//	// SRS - Calculate tDiff as time between frames vs. rendering time for iOS/macOS displayLink-driven examples project
//	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tPrevEnd).count();
//#else
//	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
//#endif
//	frameTimer = (float)tDiff / 1000.0f;
//	camera.update(frameTimer);
//	// Convert to clamped timer value
//	if (!paused)
//	{
//		timer += timerSpeed * frameTimer;
//		if (timer > 1.0)
//		{
//			timer -= 1.0f;
//		}
//	}
//	float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
//	if (fpsTimer > 1000.0f)
//	{
//		lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
//#if defined(_WIN32)
//		if (!settings.overlay) {
//			std::string windowTitle = getWindowTitle();
//			SetWindowText(window, windowTitle.c_str());
//		}
//#endif
//		frameCounter = 0;
//		lastTimestamp = tEnd;
//	}
//	tPrevEnd = tEnd;
//}


	// Create the per-frame (in flight) Vulkan synchronization primitives used in this example
void VulkanRender::createSynchronizationPrimitives()
{
	// Fences are used to check draw command buffer completion on the host
	for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
	{
		VkFenceCreateInfo fenceCI{};
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// Create the fences in signaled state (so we don't wait on first render of each command buffer)
		fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		// Fence used to ensure that command buffer has completed exection before using it again
		VK_CHECK_RESULT(vkCreateFence(vulkDevice, &fenceCI, nullptr, &vulkWaitFences[i]));
	}
	// Semaphores are used for correct command ordering within a queue
	// Used to ensure that image presentation is complete before starting to submit again
	m_presentCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);
	for (auto& semaphore : m_presentCompleteSemaphores)
	{
		VkSemaphoreCreateInfo semaphoreCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK_RESULT(vkCreateSemaphore(vulkDevice, &semaphoreCI, nullptr, &semaphore));
	}
	// Render completion
	// Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	m_renderCompleteSemaphores.resize(m_swapChain.images.size());
	for (auto& semaphore : m_renderCompleteSemaphores)
	{
		VkSemaphoreCreateInfo semaphoreCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK_RESULT(vkCreateSemaphore(vulkDevice, &semaphoreCI, nullptr, &semaphore));
	}
}

void VulkanRender::createSwapChain()
{
	m_swapChain.create(width, height, true/*settings.vsync*/, false/*settings.fullscreen*/);
}

void VulkanRender::createCommandBuffers()
{
	// All command buffers are allocated from a command pool
	VkCommandPoolCreateInfo commandPoolCI{};
	commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.queueFamilyIndex = m_swapChain.queueNodeIndex;
	commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(vulkDevice, &commandPoolCI, nullptr, &vulkCommandPool));

	// Allocate one command buffer per max. concurrent frame from above pool
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(vulkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, MAX_CONCURRENT_FRAMES);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(vulkDevice, &cmdBufAllocateInfo, vulkCommandBuffers.data()));
}

void VulkanRender::setupFrameBuffer()
{
	// Create frame buffers for every swap chain image, only one depth/stencil attachment is required, as this is owned by the application
	vulkFrameBuffers.resize(m_swapChain.images.size());
	for (uint32_t i = 0; i < vulkFrameBuffers.size(); i++)
	{
		const VkImageView attachments[2] = { m_swapChain.imageViews[i], depthStencil.view };
		VkFramebufferCreateInfo frameBufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = vulkRenderPass,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.width = width,
			.height = height,
			.layers = 1
		};
		VK_CHECK_RESULT(vkCreateFramebuffer(vulkDevice, &frameBufferCreateInfo, nullptr, &vulkFrameBuffers[i]));
	}
}

// Render pass setup
// Render passes are a new concept in Vulkan. They describe the attachments used during rendering and may contain multiple subpasses with attachment dependencies
// This allows the driver to know up-front what the rendering will look like and is a good opportunity to optimize especially on tile-based renderers (with multiple subpasses)
// Using sub pass dependencies also adds implicit layout transitions for the attachment used, so we don't need to add explicit image memory barriers to transform them
// Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
void VulkanRender::setupRenderPass()
{
	// This example will use a single render pass with one subpass

	// Descriptors for the attachments used by this renderpass
	std::array<VkAttachmentDescription, 2> attachments{};

	// Color attachment
	attachments[0].format = m_swapChain.colorFormat;                                  // Use the color format selected by the swapchain
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;                                 // We don't use multi sampling in this example
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                            // Clear this attachment at the start of the render pass
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;                          // Keep its contents after the render pass is finished (for displaying it)
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                 // We don't use stencil, so don't care for load
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;               // Same for store
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                       // Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                   // Layout to which the attachment is transitioned when the render pass is finished
	// As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR
// Depth attachment
	attachments[1].format = vulkDepthFormat;                                           // A proper depth format is selected in the example base
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                           // Clear depth at start of first subpass
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;                     // We don't need depth after render pass has finished (DONT_CARE may result in better performance)
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                // No stencil
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;              // No Stencil
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                      // Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Transition to depth/stencil attachment

	// Setup attachment references
	VkAttachmentReference colorReference{};
	colorReference.attachment = 0;                                    // Attachment 0 is color
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Attachment layout used as color during the subpass

	VkAttachmentReference depthReference{};
	depthReference.attachment = 1;                                            // Attachment 1 is color
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Attachment used as depth/stencil used during the subpass

	// Setup a single subpass reference
	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;                            // Subpass uses one color attachment
	subpassDescription.pColorAttachments = &colorReference;                 // Reference to the color attachment in slot 0
	subpassDescription.pDepthStencilAttachment = &depthReference;           // Reference to the depth attachment in slot 1
	subpassDescription.inputAttachmentCount = 0;                            // Input attachments can be used to sample from contents of a previous subpass
	subpassDescription.pInputAttachments = nullptr;                         // (Input attachments not used by this example)
	subpassDescription.preserveAttachmentCount = 0;                         // Preserved attachments can be used to loop (and preserve) attachments through subpasses
	subpassDescription.pPreserveAttachments = nullptr;                      // (Preserve attachments not used by this example)
	subpassDescription.pResolveAttachments = nullptr;                       // Resolve attachments are resolved at the end of a sub pass and can be used for e.g. multi sampling

	// Setup subpass dependencies
	// These will add the implicit attachment layout transitions specified by the attachment descriptions
	// The actual usage layout is preserved through the layout specified in the attachment reference
	// Each subpass dependency will introduce a memory and execution dependency between the source and dest subpass described by
	// srcStageMask, dstStageMask, srcAccessMask, dstAccessMask (and dependencyFlags is set)
	// Note: VK_SUBPASS_EXTERNAL is a special constant that refers to all commands executed outside of the actual renderpass)
	std::array<VkSubpassDependency, 2> dependencies{};

	// Does the transition from final to initial layout for the depth an color attachments
	// Depth attachment
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;
	// Color attachment
	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	// Create the actual renderpass
	VkRenderPassCreateInfo renderPassCI{};
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCI.attachmentCount = static_cast<uint32_t>(attachments.size());  // Number of attachments used by this render pass
	renderPassCI.pAttachments = attachments.data();                            // Descriptions of the attachments used by the render pass
	renderPassCI.subpassCount = 1;                                             // We only use one subpass in this example
	renderPassCI.pSubpasses = &subpassDescription;                             // Description of that subpass
	renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size()); // Number of subpass dependencies
	renderPassCI.pDependencies = dependencies.data();                          // Subpass dependencies used by the render pass
	VK_CHECK_RESULT(vkCreateRenderPass(vulkDevice, &renderPassCI, nullptr, &vulkRenderPass));
}

void VulkanRender::setupDepthStencil()
{
	// Create an optimal image used as the depth stencil attachment
	VkImageCreateInfo imageCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = vulkDepthFormat,
		.extent = { width, height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
	};
	VK_CHECK_RESULT(vkCreateImage(vulkDevice, &imageCI, nullptr, &depthStencil.image));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(vulkDevice, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(vulkDevice, &memAllloc, nullptr, &depthStencil.memory));
	VK_CHECK_RESULT(vkBindImageMemory(vulkDevice, depthStencil.image, depthStencil.memory, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo depthStencilViewCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depthStencil.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vulkDepthFormat,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (vulkDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		depthStencilViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	VK_CHECK_RESULT(vkCreateImageView(vulkDevice, &depthStencilViewCI, nullptr, &depthStencil.view));
}

uint32_t VulkanRender::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	// Iterate over all memory types available for the device used in this example
	for (uint32_t i = 0; i < vulkDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((vulkDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}

	throw "Could not find a suitable memory type!";
}

void VulkanRender::createUniformBuffers()
{
	// Prepare and initialize the per-frame uniform buffer blocks containing shader uniforms
	// Single uniforms like in OpenGL are no longer present in Vulkan. All hader uniforms are passed via uniform buffer blocks
	VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo{};
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(ShaderData);
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create the buffers
	for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
		VK_CHECK_RESULT(vkCreateBuffer(vulkDevice, &bufferInfo, nullptr, &m_uniformBuffers[i].buffer));
		// Get memory requirements including size, alignment and memory type
		vkGetBufferMemoryRequirements(vulkDevice, m_uniformBuffers[i].buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visible memory access
		// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
		// We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
		// Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(vulkDevice, &allocInfo, nullptr, &(m_uniformBuffers[i].memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(vulkDevice, m_uniformBuffers[i].buffer, m_uniformBuffers[i].memory, 0));
		// We map the buffer once, so we can update it without having to map it again
		VK_CHECK_RESULT(vkMapMemory(vulkDevice, m_uniformBuffers[i].memory, 0, sizeof(ShaderData), 0, (void**)&m_uniformBuffers[i].mapped));
	}

}


void VulkanRender::updateViewMatrix()
{
	glm::mat4 currentMatrix = m_viewMatrix;

	glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 position = glm::vec3(0.0f, 0.0f, -2.5f);

	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = position;
	transM = glm::translate(glm::mat4(1.0f), translation);

	m_viewMatrix = transM * rotM;
};

#pragma endregion Internal