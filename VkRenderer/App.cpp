#include "App.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <set>
#include <chrono>
#include <fstream>
#include <limits>
#include <iostream>
#include <memory>
#include <exception>
#include <stdexcept>

#include "VulkanHelper.hpp"

NAMESPACE_BEGIN(GLOBAL_NAMESPACE)

void App::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Destroy();
}

/** App */void App::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_pWindow = glfwCreateWindow(m_InitWidth, m_InitHeight, m_Title.c_str(), nullptr, nullptr);

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizeCallback);
	glfwSetMouseButtonCallback(m_pWindow, MouseButtonCallback);
	glfwSetCursorPosCallback(m_pWindow, MousePositionCallback);
	glfwSetScrollCallback(m_pWindow, MouseScrollCallback);
	glfwSetKeyCallback(m_pWindow, KeyboardCallback);
}

/** App */void App::InitVulkan()
{
	CreateInstance();

	SetupDebugMessenger();

	CreateSurface();

	SelectPhysicalDevice();

	CreateLogicalDevice();

	CreateSwapChain();

	CreateSwapChainImageViews();

	CreateRenderPass();

	CreateDescriptorSetLayout();

	CreateGraphicsPipeline();

	CreateCommandPool();

	CreateColorResource();

	CreateDepthResource();

	CreateFramebuffers();

	LoadAndCreateTextureImage();

	CreateTextureImageView();

	CreateTextureSampler();

	LoadObjModel();

	CreateVertexBuffer();

	CreateIndexBuffer();

	CreateMvpUniformBuffer();

	CreateLightUniformBuffer();

	CreateMaterialUniformBuffer();

	CreateDescriptorPool();

	CreateDescriptorSets();

	CreateDrawingCommandBuffers();

	CreateSyncObjects();
}

/** App */void App::MainLoop()
{
	static uint64_t Frame = 0;
	static auto PrevTime = std::chrono::high_resolution_clock::now();
	static auto CurrTime = std::chrono::high_resolution_clock::now();
	static double DeltaTime = 0.0f;
	static constexpr double TitleUpdateTime = 1.0 / 10.0;

	while (!glfwWindowShouldClose(m_pWindow))
	{
		glfwPollEvents();
		Draw();

		Frame++;
		CurrTime = std::chrono::high_resolution_clock::now();
		DeltaTime = std::chrono::duration<double, std::chrono::seconds::period>(
			CurrTime - PrevTime
		).count();

		if (DeltaTime >= TitleUpdateTime)
		{
			m_FPS = static_cast<double>(Frame) / DeltaTime;
			PrevTime = CurrTime;
			Frame = 0;

			glm::vec3 Eye = m_Camera.GetCachedEye();

			char Buffer[256];
			sprintf_s(
				Buffer, "%s [%s] [Vertex : %d Facet : %d] [Eye : (%.2f, %.2f, %.2f)] [%s] Fps: %d", 
				m_Title.c_str(), 
				m_GpuName.c_str(),
				static_cast<int32_t>(m_VertexNum), 
				static_cast<int32_t>(m_FacetNum),
				Eye.x, Eye.y, Eye.z,
				m_GraphicsPipelinesDescription[m_GraphicsPipelineDisplayMode | m_GraphicsPipelineCullMode],
				static_cast<int32_t>(m_FPS)
			);
			glfwSetWindowTitle(m_pWindow, Buffer);
		}
	}

	vkDeviceWaitIdle(m_Device);
}

/** App */void App::Draw()
{
	vkWaitForFences(
		m_Device, 
		1, 
		&m_InFlightFences[m_CurrentFrame], 
		VK_TRUE, 
		std::numeric_limits<uint64_t>::max()
	);

	uint32_t ImageIndex;
	VkResult Result = vkAcquireNextImageKHR(
		m_Device, m_SwapChain, 
		std::numeric_limits<uint64_t>::max(), 
		m_ImageAvailableSemaphores[m_CurrentFrame], 
		VK_NULL_HANDLE, 
		&ImageIndex
	);

	if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
	{
		RecreateSwapChainAndRelevantObject();
		return;
	}
	else if (Result != VK_SUCCESS)
	{
		std::runtime_error("Failed to acquire swap chain image!");
	}

	UpdateUniformBuffer(ImageIndex);

	VkSubmitInfo SubmitInfo = {};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore WaitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
	VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	SubmitInfo.waitSemaphoreCount = 1;
	SubmitInfo.pWaitSemaphores = WaitSemaphores;
	SubmitInfo.pWaitDstStageMask = WaitStages;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &m_DrawingCommandBuffers[ImageIndex];

	VkSemaphore SignalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
	SubmitInfo.signalSemaphoreCount = 1;
	SubmitInfo.pSignalSemaphores = SignalSemaphores;

	vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

	if (vkQueueSubmit(m_GraphicsQueue, 1, &SubmitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	VkPresentInfoKHR PresentInfo = {};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = SignalSemaphores;

	VkSwapchainKHR SwapChains[] = { m_SwapChain };
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = SwapChains;
	PresentInfo.pImageIndices = &ImageIndex;
	PresentInfo.pResults = nullptr;

	Result = vkQueuePresentKHR(m_PresentQueue, &PresentInfo);

	if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR || m_bFramebufferResized)
	{
		m_bFramebufferResized = false;
		RecreateSwapChainAndRelevantObject();
		return;
	}
	else if (Result != VK_SUCCESS)
	{
		std::runtime_error("Failed to acquire swap chain image!");
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % m_MaxFramesInFlights;
}

/** App */void App::Destroy()
{
	for (size_t i = 0; i < m_MaxFramesInFlights; i++)
	{
		vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
	}

	DestroySwapChainAndRelevantObject();

	vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		vkDestroyBuffer(m_Device, m_MaterialUniformBuffers[i], nullptr);
		vkFreeMemory(m_Device, m_MaterialUniformBufferMemories[i], nullptr);
	}

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		vkDestroyBuffer(m_Device, m_LightUniformBuffers[i], nullptr);
		vkFreeMemory(m_Device, m_LightUniformBufferMemories[i], nullptr);
	}

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		vkDestroyBuffer(m_Device, m_MvpUniformBuffers[i], nullptr);
		vkFreeMemory(m_Device, m_MvpUniformBufferMemories[i], nullptr);
	}

	vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
	vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

	vkDestroySampler(m_Device, m_AlbedoTextureSamler, nullptr);
	vkDestroyImageView(m_Device, m_AlbedoTextureImageView, nullptr);
	vkDestroyImage(m_Device, m_AlbedoTextureImage, nullptr);
	vkFreeMemory(m_Device, m_AlbedoTextureImageMemory, nullptr);

	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
	
	vkDestroyDevice(m_Device, nullptr);
	
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	
	if (m_bEnableValidationLayers)
	{
		ProxyVulkanFunction::vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}
	
	vkDestroyInstance(m_Instance, nullptr);

	glfwDestroyWindow(m_pWindow);
	
	glfwTerminate();
}

/** App Helper */void App::UpdateUniformBuffer(
	uint32_t CurrentImage
)
{
	/** Update MVP matrix */
	static auto StartTime = std::chrono::high_resolution_clock::now();
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	float DeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(CurrentTime - StartTime).count();
	DeltaTime *= 10.0f;

	static glm::vec3 Eye, Target, Up;
	static float NearZ, FarZ;
	static glm::vec2 Fov;

	m_Camera.RetriveData(Target, Eye, Up, Fov, NearZ, FarZ);

	MvpUniformBufferObject Transformation = {};
	Transformation.Model = glm::mat4(1.0f);
	Transformation.View = glm::lookAt(Eye, Target, Up);
	Transformation.Projection = glm::perspective(
		Fov.y,
		static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height), 
		NearZ, 
		FarZ
	);
	/** GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted */
	Transformation.Projection[1][1] *= -1.0f;

	void * pData = nullptr;
	vkMapMemory(m_Device, m_MvpUniformBufferMemories[CurrentImage], 0, sizeof(Transformation), 0, &pData);
	memcpy(pData, &Transformation, sizeof(Transformation));
	vkUnmapMemory(m_Device, m_MvpUniformBufferMemories[CurrentImage]);

	/** Update light information */
	LightUniformBufferObject Lighting = {};
	Lighting.LightPosition[0] = glm::vec4(-2.0, -2.0, 2.0f, 1.0f);
	Lighting.LightPosition[1] = glm::vec4(2.0, -2.0, 2.0f, 1.0f);
	Lighting.LightPosition[2] = glm::vec4(-2.0, 2.0, 2.0f, 1.0f);
	Lighting.LightPosition[3] = glm::vec4(2.0, 2.0, 2.0f, 1.0f);
	Lighting.LightPosition[4] = glm::vec4(-2.0, -2.0, -2.0f, 1.0f);
	Lighting.LightPosition[5] = glm::vec4(2.0, -2.0, -2.0f, 1.0f);
	Lighting.LightPosition[6] = glm::vec4(-2.0, 2.0, -2.0f, 1.0f);
	Lighting.LightPosition[7] = glm::vec4(2.0, 2.0, -2.0f, 1.0f);
	Lighting.LightColor[0] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[1] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[2] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[3] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[4] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[5] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[6] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.LightColor[7] = glm::vec4(12.0f, 12.0f, 12.0f, 1.0f);
	Lighting.ViewPosition = m_Camera.GetCachedEye();

	pData = nullptr;
	vkMapMemory(m_Device, m_LightUniformBufferMemories[CurrentImage], 0, sizeof(Lighting), 0, &pData);
	memcpy(pData, &Lighting, sizeof(Lighting));
	vkUnmapMemory(m_Device, m_LightUniformBufferMemories[CurrentImage]);

	/** Update material information */
	MaterialUniformBufferObject Material = {};
	Material.Albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0);
	Material.Ao = 1.0f;
	Material.Metallic = 0.15f;
	Material.Roughness = 0.35f;

	pData = nullptr;
	vkMapMemory(m_Device, m_MaterialUniformBufferMemories[CurrentImage], 0, sizeof(Material), 0, &pData);
	memcpy(pData, &Material, sizeof(Material));
	vkUnmapMemory(m_Device, m_MaterialUniformBufferMemories[CurrentImage]);
}

/** App Helper */void App::RecreateSwapChainAndRelevantObject()
{
	int Width = 0, Height = 0;
	while (Width == 0 || Height == 0)
	{
		glfwGetFramebufferSize(m_pWindow, &Width, &Height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_Device);

	DestroySwapChainAndRelevantObject();

	CreateSwapChain();

	CreateSwapChainImageViews();

	CreateRenderPass();

	CreateGraphicsPipeline();

	CreateColorResource();

	CreateDepthResource();

	CreateFramebuffers();

	CreateDrawingCommandBuffers();
}

/** App Helper */void App::DestroySwapChainAndRelevantObject()
{
	vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
	vkDestroyImage(m_Device, m_DepthImage, nullptr);
	vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

	vkDestroyImageView(m_Device, m_ColorImageView, nullptr);
	vkDestroyImage(m_Device, m_ColorImage, nullptr);
	vkFreeMemory(m_Device, m_ColorImageMemory, nullptr);

	for (auto & Framebuffer : m_SwapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_Device, Framebuffer, nullptr);
	}

	for (auto & Kv : m_GraphicsPipelines)
	{
		vkDestroyPipeline(m_Device, Kv.second, nullptr);
	}

	vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

	vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

	for (auto & SwapChainImageView : m_SwapChainImageViews)
	{
		vkDestroyImageView(m_Device, SwapChainImageView, nullptr);
	}

	vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);

	vkFreeCommandBuffers(
		m_Device, 
		m_CommandPool, 
		static_cast<uint32_t>(m_DrawingCommandBuffers.size()), 
		m_DrawingCommandBuffers.data()
	);
}

void App::RecreateDrawingCommandBuffer()
{
	vkQueueWaitIdle(m_GraphicsQueue);

	vkFreeCommandBuffers(
		m_Device,
		m_CommandPool,
		static_cast<uint32_t>(m_DrawingCommandBuffers.size()),
		m_DrawingCommandBuffers.data()
	);

	CreateDrawingCommandBuffers();
}

/** Vulkan Init */void App::CreateInstance()
{
	if (m_bEnableValidationLayers && !CheckValidationLayerSupport(m_ValidationLayers))
	{
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	VkApplicationInfo AppInfo = {};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pApplicationName = m_AppName.c_str();
	AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.pEngineName = m_EngineName.c_str();
	AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &AppInfo;

	auto Extensions = GetRequiredExtensions(m_bEnableValidationLayers);

	CreateInfo.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
	CreateInfo.ppEnabledExtensionNames = Extensions.data();

	if (m_bEnableValidationLayers)
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else
	{
		CreateInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&CreateInfo, nullptr, &m_Instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create VkInstance!");
	}
}

/** Vulkan Init */void App::SetupDebugMessenger()
{
	if (!m_bEnableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	CreateInfo.messageSeverity = 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	CreateInfo.messageType = 
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	CreateInfo.pfnUserCallback = DebugCallback;
	CreateInfo.pUserData = nullptr;

	if (ProxyVulkanFunction::vkCreateDebugUtilsMessengerEXT(
		m_Instance, 
		&CreateInfo,
		nullptr, 
		&m_DebugMessenger
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}

/** Vulkan Init */void App::CreateSurface()
{
	if (glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &m_Surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

/** Vulkan Init */void App::SelectPhysicalDevice()
{
	uint32_t DeviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, nullptr);

	if (DeviceCount == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	}

	std::unique_ptr<VkPhysicalDevice[]> PhysicalDevices(new VkPhysicalDevice[DeviceCount]);
	vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, PhysicalDevices.get());

	VkDeviceSize MaxMemory = 0;
	for (uint32_t i = 0; i < DeviceCount; i++)
	{
		if (IsPhysicalDeviceSuitable(PhysicalDevices[i], m_Surface, m_DeviceExtensions))
		{
			VkPhysicalDeviceProperties PhysicalDeviceProperties;
			vkGetPhysicalDeviceProperties(PhysicalDevices[i], &PhysicalDeviceProperties);

			VkPhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties;
			vkGetPhysicalDeviceMemoryProperties(PhysicalDevices[i], &PhysicalDeviceMemoryProperties);

			VkDeviceSize Memory = 0;
			for (uint32_t i = 0; i < PhysicalDeviceMemoryProperties.memoryHeapCount; i++)
			{
				Memory += PhysicalDeviceMemoryProperties.memoryHeaps[i].size / 1024 / 1024;
			}

			/** We trend to choose discrete GPU */
			if (PhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				Memory *= 50;
			}

			if (Memory > MaxMemory)
			{
				m_PhysicalDevice = PhysicalDevices[i];
				m_MsaaSamples = GetMaxUsableSampleCount(PhysicalDevices[i]);
				m_GpuName = PhysicalDeviceProperties.deviceName;
				MaxMemory = Memory;
			}
		}
	}

	if (m_PhysicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Failed to find a support GPU!");
	}
}

/** Vulkan Init */void App::CreateLogicalDevice()
{
	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice, m_Surface);
	float QueuePriority = 1.0f;

	std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
	std::set<uint32_t> UniqueQueueFamilies = 
	{ 
		Indices.GraphicsFamily.value(), 
		Indices.PresentFamily.value() 
	};

	for (uint32_t QueueFamily : UniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo QueueCreateInfo = {};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = QueueFamily;
		QueueCreateInfo.queueCount = 1;
		QueueCreateInfo.pQueuePriorities = &QueuePriority;
		QueueCreateInfos.push_back(QueueCreateInfo);
	}

	VkPhysicalDeviceFeatures DeviceFeatures = {};
	DeviceFeatures.samplerAnisotropy = VK_TRUE;
	DeviceFeatures.sampleRateShading = VK_TRUE;
	DeviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
	CreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
	CreateInfo.pEnabledFeatures = &DeviceFeatures;
	CreateInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
	CreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());

	if (m_bEnableValidationLayers)
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else
	{
		CreateInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_PhysicalDevice, &CreateInfo, nullptr, &m_Device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device!");
	}

	vkGetDeviceQueue(m_Device, Indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, Indices.PresentFamily.value(), 0, &m_PresentQueue);
}

/** Vulkan Init */void App::CreateSwapChain()
{
	SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(m_PhysicalDevice, m_Surface);
	VkSurfaceFormatKHR SurfaceFormat = ChooseSwapSurfaceFormat(SwapChainSupport.Formats);
	VkPresentModeKHR PresentMode = ChooseSwapPresentMode(SwapChainSupport.PresentModes);
	VkExtent2D Extent = ChooseSwapExtent(m_pWindow, SwapChainSupport.Capabilities, m_InitWidth, m_InitHeight);

	uint32_t ImageCount = SwapChainSupport.Capabilities.minImageCount + 1;
	if (SwapChainSupport.Capabilities.maxImageCount > 0 &&
		ImageCount > SwapChainSupport.Capabilities.maxImageCount)
	{
		ImageCount = SwapChainSupport.Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	CreateInfo.surface = m_Surface;
	CreateInfo.minImageCount = ImageCount;
	CreateInfo.imageFormat = SurfaceFormat.format;
	CreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
	CreateInfo.imageExtent = Extent;
	CreateInfo.imageArrayLayers = 1;
	CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice, m_Surface);
	uint32_t QueueFamilyIndices[] = 
	{ 
		Indices.GraphicsFamily.value(),
		Indices.PresentFamily.value() 
	};

	if (Indices.GraphicsFamily != Indices.PresentFamily)
	{
		CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		CreateInfo.queueFamilyIndexCount = 2;
		CreateInfo.pQueueFamilyIndices = QueueFamilyIndices;
	}
	else
	{
		CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CreateInfo.queueFamilyIndexCount = 0;
		CreateInfo.pQueueFamilyIndices = nullptr;
	}

	CreateInfo.preTransform = SwapChainSupport.Capabilities.currentTransform;
	CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	CreateInfo.presentMode = PresentMode;
	CreateInfo.clipped = VK_TRUE;
	CreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_Device, &CreateInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &ImageCount, nullptr);
	m_SwapChainImages.resize(ImageCount);
	vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &ImageCount, m_SwapChainImages.data());

	m_SwapChainImageFormat = SurfaceFormat.format;
	m_SwapChainExtent = Extent;
}

/** Vulkan Init */void App::CreateSwapChainImageViews()
{
	m_SwapChainImageViews.resize(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		CreateImageView(
			m_Device, 
			m_SwapChainImages[i], 
			m_SwapChainImageFormat, 
			1,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_SwapChainImageViews[i]
		);
	}
}

/** Vulkan Init */void App::CreateRenderPass()
{
	VkAttachmentDescription ColorAttachment = {};
	ColorAttachment.format = m_SwapChainImageFormat;
	ColorAttachment.samples = m_MsaaSamples;
	ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription DepthAttachment = {};
	DepthAttachment.format = FindDepthFormat(m_PhysicalDevice);
	DepthAttachment.samples = m_MsaaSamples;
	DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription ColorAttachmentResolve = {};
	ColorAttachmentResolve.format = m_SwapChainImageFormat;
	ColorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	ColorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ColorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ColorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ColorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ColorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ColorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference ColorAttachmentRef = {};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference DepthAttachmentRef = {};
	DepthAttachmentRef.attachment = 1;
	DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference ColorAttachmentResolveRef = {};
	ColorAttachmentResolveRef.attachment = 2;
	ColorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;
	Subpass.pDepthStencilAttachment = &DepthAttachmentRef;
	Subpass.pResolveAttachments = &ColorAttachmentResolveRef;

	std::array<VkAttachmentDescription, 3> Attachments = 
	{
		ColorAttachment, DepthAttachment, ColorAttachmentResolve
	};

	VkSubpassDependency Dependency = {};
	Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	Dependency.dstSubpass = 0;
	Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	Dependency.srcAccessMask = 0;
	Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	CreateInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
	CreateInfo.pAttachments = Attachments.data();
	CreateInfo.subpassCount = 1;
	CreateInfo.pSubpasses = &Subpass;
	CreateInfo.dependencyCount = 1;
	CreateInfo.pDependencies = &Dependency;

	if (vkCreateRenderPass(m_Device, &CreateInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass!");
	}
}

/** Vulkan Init */void App::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding MvpUboLayoutBinding = {};
	MvpUboLayoutBinding.binding = 0;
	MvpUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MvpUboLayoutBinding.descriptorCount = 1;
	MvpUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	MvpUboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding LightUboLayoutBinding = {};
	LightUboLayoutBinding.binding = 1;
	LightUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	LightUboLayoutBinding.descriptorCount = 1;
	LightUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	LightUboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding MaterialUboLayoutBinding = {};
	MaterialUboLayoutBinding.binding = 2;
	MaterialUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MaterialUboLayoutBinding.descriptorCount = 1;
	MaterialUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	MaterialUboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding SamplerLayoutBinding = {};
	SamplerLayoutBinding.binding = 3;
	SamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	SamplerLayoutBinding.descriptorCount = 1;
	SamplerLayoutBinding.pImmutableSamplers = nullptr;
	SamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 4> Bindings =
	{
		MvpUboLayoutBinding,
		LightUboLayoutBinding,
		MaterialUboLayoutBinding,
		SamplerLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo LayoutCreateInfo = {};
	LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	LayoutCreateInfo.bindingCount = static_cast<uint32_t>(Bindings.size());
	LayoutCreateInfo.pBindings = Bindings.data();

	if (vkCreateDescriptorSetLayout(
		m_Device, 
		&LayoutCreateInfo, 
		nullptr, 
		&m_DescriptorSetLayout
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout!");
	}
}

/** Vulkan Init */void App::CreateGraphicsPipeline()
{
	// Shader modules
	auto VertShaderCode = ReadFile(m_VertexShaderPath);
	auto FragShaderCode = ReadFile(m_FragmentShaderPath);

	VkShaderModule VertShaderModule = CreateShaderModule(m_Device, VertShaderCode);
	VkShaderModule FragShaderModule = CreateShaderModule(m_Device, FragShaderCode);

	VkPipelineShaderStageCreateInfo VertShaderStageCreateInfo = {};
	VertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	VertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	VertShaderStageCreateInfo.module = VertShaderModule;
	VertShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo FragShaderStageCreateInfo = {};
	FragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	FragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	FragShaderStageCreateInfo.module = FragShaderModule;
	FragShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo ShaderStageCreateInfos[] = 
	{ 
		VertShaderStageCreateInfo,
		FragShaderStageCreateInfo
	};

	// Vertex input
	auto BindingDescription = Vertex::GetBindingDescription();
	auto AttributeDescription = Vertex::GetAttributeDescription();

	VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {};
	VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	VertexInputStateCreateInfo.pVertexBindingDescriptions = &BindingDescription;
	VertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(AttributeDescription.size());
	VertexInputStateCreateInfo.pVertexAttributeDescriptions = AttributeDescription.data();

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {};
	InputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	InputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	// Viewports and scissors
	VkViewport Viewport = {};
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.width = static_cast<float>(m_SwapChainExtent.width);
	Viewport.height = static_cast<float>(m_SwapChainExtent.height);
	Viewport.minDepth = 0.0f;
	Viewport.maxDepth = 1.0f;

	VkRect2D Scissor = {};
	Scissor.offset = { 0, 0 };
	Scissor.extent = m_SwapChainExtent;

	VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
	ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportStateCreateInfo.viewportCount = 1;
	ViewportStateCreateInfo.pViewports = &Viewport;
	ViewportStateCreateInfo.scissorCount = 1;
	ViewportStateCreateInfo.pScissors = &Scissor;

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {};
	RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	RasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	RasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	RasterizationStateCreateInfo.lineWidth = 1.0f;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	RasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	RasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	RasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	RasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	// Multisampling
	VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {};
	MultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	MultisampleStateCreateInfo.sampleShadingEnable = VK_TRUE;
	MultisampleStateCreateInfo.rasterizationSamples = m_MsaaSamples;
	MultisampleStateCreateInfo.minSampleShading = 1.0f;
	MultisampleStateCreateInfo.pSampleMask = nullptr;
	MultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	MultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	// Depth and stencil testing
	VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo = {};
	DepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DepthStencilCreateInfo.depthTestEnable = VK_TRUE;
	DepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	DepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	DepthStencilCreateInfo.minDepthBounds = 0.0f;
	DepthStencilCreateInfo.maxDepthBounds = 1.0f;
	DepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	DepthStencilCreateInfo.front = {};
	DepthStencilCreateInfo.back = {};

	// Color blending
	VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
	ColorBlendAttachmentState.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | 
		VK_COLOR_COMPONENT_A_BIT;
	ColorBlendAttachmentState.blendEnable = VK_FALSE;
	ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
	ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	ColorBlendStateCreateInfo.attachmentCount = 1;
	ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachmentState;
	ColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	ColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	ColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	ColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	// Dynamic state
	// TODO : Use it later...

	// Pipeline layout
	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};
	PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	PipelineLayoutCreateInfo.setLayoutCount = 1;
	PipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;
	PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	PipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_Device, &PipelineLayoutCreateInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo = {};
	GraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	GraphicsPipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	GraphicsPipelineCreateInfo.stageCount = 2;
	GraphicsPipelineCreateInfo.pStages = ShaderStageCreateInfos;
	GraphicsPipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;
	GraphicsPipelineCreateInfo.pInputAssemblyState = &InputAssemblyStateCreateInfo;
	GraphicsPipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
	GraphicsPipelineCreateInfo.pRasterizationState = &RasterizationStateCreateInfo;
	GraphicsPipelineCreateInfo.pMultisampleState = &MultisampleStateCreateInfo;
	GraphicsPipelineCreateInfo.pDepthStencilState = &DepthStencilCreateInfo;
	GraphicsPipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
	GraphicsPipelineCreateInfo.pDynamicState = nullptr;
	GraphicsPipelineCreateInfo.layout = m_PipelineLayout;
	GraphicsPipelineCreateInfo.renderPass = m_RenderPass;
	GraphicsPipelineCreateInfo.subpass = 0;
	GraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	GraphicsPipelineCreateInfo.basePipelineIndex = -1;

	/** GRAPHICS_PIPELINE_TYPE_FILL & GRAPHICS_PIPELINE_TYPE_FRONT_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE, 
		1, 
		&GraphicsPipelineCreateInfo,
		nullptr, 
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_FILL | GRAPHICS_PIPELINE_TYPE_FRONT_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/

	/** GRAPHICS_PIPELINE_TYPE_WIREFRAME & GRAPHICS_PIPELINE_TYPE_FRONT_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_WIREFRAME | GRAPHICS_PIPELINE_TYPE_FRONT_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/

	/** GRAPHICS_PIPELINE_TYPE_POINT & GRAPHICS_PIPELINE_TYPE_FRONT_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_POINT;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_POINT | GRAPHICS_PIPELINE_TYPE_FRONT_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_FILL & GRAPHICS_PIPELINE_TYPE_BACK_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_FILL | GRAPHICS_PIPELINE_TYPE_BACK_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_WIREFRAME & GRAPHICS_PIPELINE_TYPE_BACK_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_WIREFRAME | GRAPHICS_PIPELINE_TYPE_BACK_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_POINT & GRAPHICS_PIPELINE_TYPE_BACK_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_POINT;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_POINT | GRAPHICS_PIPELINE_TYPE_BACK_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_FILL & GRAPHICS_PIPELINE_TYPE_NONE_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_FILL | GRAPHICS_PIPELINE_TYPE_NONE_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_WIREFRAME & GRAPHICS_PIPELINE_TYPE_NONE_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_WIREFRAME | GRAPHICS_PIPELINE_TYPE_NONE_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/
	
	/** GRAPHICS_PIPELINE_TYPE_POINT & GRAPHICS_PIPELINE_TYPE_NONE_CULL */
	RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_POINT;
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE,
		1,
		&GraphicsPipelineCreateInfo,
		nullptr,
		&m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_POINT | GRAPHICS_PIPELINE_TYPE_NONE_CULL]
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
	/************************************************************************/

	vkDestroyShaderModule(m_Device, VertShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, FragShaderModule, nullptr);
}

/** Vulkan Init */void App::CreateCommandPool()
{
	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice, m_Surface);

	VkCommandPoolCreateInfo CmdPoolCreateInfo = {};
	CmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	CmdPoolCreateInfo.queueFamilyIndex = Indices.GraphicsFamily.value();
	CmdPoolCreateInfo.flags = 0;

	if (vkCreateCommandPool(m_Device, &CmdPoolCreateInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

/** Vulkan Init */void App::CreateColorResource()
{
	VkFormat ColorFormat = m_SwapChainImageFormat;

	CreateImage(
		m_PhysicalDevice,
		m_Device,
		m_SwapChainExtent.width,
		m_SwapChainExtent.height,
		1,
		m_MsaaSamples,
		ColorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_ColorImage,
		m_ColorImageMemory
	);

	CreateImageView(
		m_Device, 
		m_ColorImage,
		ColorFormat, 
		1, 
		VK_IMAGE_ASPECT_COLOR_BIT,
		m_ColorImageView
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_ColorImage,
		ColorFormat,
		1,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);
}

/** Vulkan Init */void App::CreateDepthResource()
{
	VkFormat DepthFormat = FindDepthFormat(m_PhysicalDevice);
	CreateImage(
		m_PhysicalDevice,
		m_Device,
		m_SwapChainExtent.width,
		m_SwapChainExtent.height,
		1,
		m_MsaaSamples,
		DepthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_DepthImage,
		m_DepthImageMemory
	);

	CreateImageView(
		m_Device,
		m_DepthImage,
		DepthFormat,
		1,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		m_DepthImageView
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_DepthImage,
		DepthFormat,
		1,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);
}

/** Vulkan Init */void App::CreateFramebuffers()
{
	m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

	for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
	{
		std::array<VkImageView, 3> Attachments =
		{
			m_ColorImageView,
			m_DepthImageView,
			m_SwapChainImageViews[i]
		};

		VkFramebufferCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		CreateInfo.renderPass = m_RenderPass;
		CreateInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
		CreateInfo.pAttachments = Attachments.data();
		CreateInfo.width = m_SwapChainExtent.width;
		CreateInfo.height = m_SwapChainExtent.height;
		CreateInfo.layers = 1;

		if (vkCreateFramebuffer(m_Device, &CreateInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

/** Vulkan Init */void App::LoadAndCreateTextureImage()
{
	int TexWidth = -1, TexHeight = -1, TexChannels = -1;
	stbi_uc * pPixels = stbi_load(
		m_AlbedoTexturePath.c_str(),
		&TexWidth, 
		&TexHeight, 
		&TexChannels,
		STBI_rgb_alpha
	);
	VkDeviceSize ImageSize = TexWidth * TexHeight * 4;
	m_AlbedoMipLevels = static_cast<uint32_t>(
		std::floor(std::log2(std::max(TexWidth, TexHeight)))
	) + 1;

	if (pPixels == nullptr)
	{
		throw std::runtime_error("Failed to load texture image!");
	}

	VkBuffer StagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;

	CreateBuffer(
		m_PhysicalDevice,
		m_Device,
		ImageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		StagingBuffer,
		StagingBufferMemory
	);

	void * pData = nullptr;
	vkMapMemory(m_Device, StagingBufferMemory, 0, ImageSize, 0, &pData);
	memcpy(pData, pPixels, static_cast<size_t>(ImageSize));
	vkUnmapMemory(m_Device, StagingBufferMemory);

	stbi_image_free(pPixels);

	CreateImage(
		m_PhysicalDevice,
		m_Device,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight),
		m_AlbedoMipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_AlbedoTextureImage,
		m_AlbedoTextureImageMemory
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_AlbedoTextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		m_AlbedoMipLevels,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	CopyBufferToImage(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		StagingBuffer,
		m_AlbedoTextureImage,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight)
	);

	GenerateMipmaps(
		m_PhysicalDevice,
		m_Device,
		m_CommandPool,
		m_GraphicsQueue,
		m_AlbedoTextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		TexWidth,
		TexHeight,
		m_AlbedoMipLevels
	);

	vkDestroyBuffer(m_Device, StagingBuffer, nullptr);
	vkFreeMemory(m_Device, StagingBufferMemory, nullptr);
}

/** Vulkan Init */void App::CreateTextureImageView()
{
	CreateImageView(
		m_Device, 
		m_AlbedoTextureImage, 
		VK_FORMAT_R8G8B8A8_UNORM,
		m_AlbedoMipLevels,
		VK_IMAGE_ASPECT_COLOR_BIT,
		m_AlbedoTextureImageView
	);
}

/** Vulkan Init */void App::CreateTextureSampler()
{
	VkSamplerCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	CreateInfo.magFilter = VK_FILTER_LINEAR;
	CreateInfo.minFilter = VK_FILTER_LINEAR;
	CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.anisotropyEnable = VK_TRUE;
	CreateInfo.maxAnisotropy = 16;
	CreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	CreateInfo.unnormalizedCoordinates = VK_FALSE;
	CreateInfo.compareEnable = VK_FALSE;
	CreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	CreateInfo.mipLodBias = 0.0f;
	CreateInfo.minLod = 0.0f;
	CreateInfo.maxLod = static_cast<float>(m_AlbedoMipLevels);

	if (vkCreateSampler(m_Device, &CreateInfo, nullptr, &m_AlbedoTextureSamler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler!");
	}
}

void App::LoadObjModel()
{
	tinyobj::attrib_t Attrib;
	std::vector<tinyobj::shape_t> Shapes;
	std::vector<tinyobj::material_t> Materials;
	std::string Warn, Error;
	std::unordered_map<Vertex, uint32_t, VertexHash, VertexEqual> UniqueVertices = {};

	m_Vertices.clear();
	m_Vertices.shrink_to_fit();
	m_Indices.clear();
	m_Indices.shrink_to_fit();

	if (!tinyobj::LoadObj(&Attrib, &Shapes, &Materials, &Warn, &Error, m_ModelPath.c_str()))
	{
		throw std::runtime_error(Warn + Error);
	}

	for (const auto & Shape : Shapes)
	{
		for (const auto & Index : Shape.mesh.indices)
		{
			Vertex Vertex = {};

			Vertex.Position =
			{
				Attrib.vertices[3 * Index.vertex_index + 0],
				Attrib.vertices[3 * Index.vertex_index + 1],
				Attrib.vertices[3 * Index.vertex_index + 2]
			};

			if (Attrib.colors.size() != 0)
			{
				Vertex.Color =
				{
					Attrib.colors[3 * Index.vertex_index + 0],
					Attrib.colors[3 * Index.vertex_index + 1],
					Attrib.colors[3 * Index.vertex_index + 2]
				};
			}
			else
			{
				Vertex.Color = { 1.0f, 1.0f, 1.0f };
			}

			if (Attrib.normals.size() != 0)
			{
				Vertex.Normal =
				{
					Attrib.normals[3 * Index.normal_index + 0],
					Attrib.normals[3 * Index.normal_index + 1],
					Attrib.normals[3 * Index.normal_index + 2]
				};
			}

			if (Attrib.texcoords.size() != 0)
			{
				Vertex.TexCoord =
				{
					Attrib.texcoords[2 * Index.texcoord_index + 0],
					Attrib.texcoords[2 * Index.texcoord_index + 1]
				};
			}

			/**
			 * The problem is that the origin of texture coordinates in Vulkan is the top-left corner, 
			 * whereas the OBJ format assumes the bottom-left corner. 
			 */
			Vertex.TexCoord.y = 1.0f - Vertex.TexCoord.y;

			if (UniqueVertices.count(Vertex) == 0)
			{
				UniqueVertices[Vertex] = static_cast<uint32_t>(m_Vertices.size());
				m_Vertices.push_back(Vertex);
			}
			m_Indices.push_back(UniqueVertices[Vertex]);
		}
	}

	m_VertexNum = m_Vertices.size();
	m_FacetNum = m_Indices.size() / 3;
}

/** Vulkan Init */void App::CreateVertexBuffer()
{
	VkDeviceSize BufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();

	VkBuffer StagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;
	CreateBuffer(
		m_PhysicalDevice,
		m_Device,
		BufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		StagingBuffer,
		StagingBufferMemory
	);

	void * pData = nullptr;
	vkMapMemory(m_Device, StagingBufferMemory, 0, BufferSize, 0, &pData);
	memcpy(pData, m_Vertices.data(), static_cast<size_t>(BufferSize));
	vkUnmapMemory(m_Device, StagingBufferMemory);

	CreateBuffer(
		m_PhysicalDevice,
		m_Device,
		BufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_VertexBuffer,
		m_VertexBufferMemory
	);

	CopyBuffer(m_Device, m_CommandPool, m_GraphicsQueue, StagingBuffer, m_VertexBuffer, BufferSize);

	vkDestroyBuffer(m_Device, StagingBuffer, nullptr);
	vkFreeMemory(m_Device, StagingBufferMemory, nullptr);
}

/** Vulkan Init */void App::CreateIndexBuffer()
{
	VkDeviceSize BufferSize = sizeof(m_Indices[0]) * m_Indices.size();

	VkBuffer StagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;
	CreateBuffer(
		m_PhysicalDevice,
		m_Device,
		BufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		StagingBuffer,
		StagingBufferMemory
	);

	void * pData = nullptr;
	vkMapMemory(m_Device, StagingBufferMemory, 0, BufferSize, 0, &pData);
	memcpy(pData, m_Indices.data(), static_cast<size_t>(BufferSize));
	vkUnmapMemory(m_Device, StagingBufferMemory);

	CreateBuffer(
		m_PhysicalDevice,
		m_Device,
		BufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_IndexBuffer,
		m_IndexBufferMemory
	);

	CopyBuffer(m_Device, m_CommandPool, m_GraphicsQueue, StagingBuffer, m_IndexBuffer, BufferSize);

	vkDestroyBuffer(m_Device, StagingBuffer, nullptr);
	vkFreeMemory(m_Device, StagingBufferMemory, nullptr);
}

/** Vulkan Init */void App::CreateMvpUniformBuffer()
{
	VkDeviceSize BufferSize = sizeof(MvpUniformBufferObject);

	m_MvpUniformBuffers.resize(m_SwapChainImages.size());
	m_MvpUniformBufferMemories.resize(m_SwapChainImages.size());

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		CreateBuffer(
			m_PhysicalDevice,
			m_Device,
			BufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_MvpUniformBuffers[i],
			m_MvpUniformBufferMemories[i]
		);
	}
}

/** Vulkan Init */void App::CreateLightUniformBuffer()
{
	VkDeviceSize BufferSize = sizeof(LightUniformBufferObject);

	m_LightUniformBuffers.resize(m_SwapChainImages.size());
	m_LightUniformBufferMemories.resize(m_SwapChainImages.size());

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		CreateBuffer(
			m_PhysicalDevice,
			m_Device,
			BufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_LightUniformBuffers[i],
			m_LightUniformBufferMemories[i]
		);
	}
}

void App::CreateMaterialUniformBuffer()
{
	VkDeviceSize BufferSize = sizeof(MaterialUniformBufferObject);

	m_MaterialUniformBuffers.resize(m_SwapChainImages.size());
	m_MaterialUniformBufferMemories.resize(m_SwapChainImages.size());

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		CreateBuffer(
			m_PhysicalDevice,
			m_Device,
			BufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_MaterialUniformBuffers[i],
			m_MaterialUniformBufferMemories[i]
		);
	}
}

/** Vulkan Init */void App::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 4> PoolSizes = {};
	
	PoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	PoolSizes[0].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());
	
	PoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	PoolSizes[1].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());

	PoolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	PoolSizes[2].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());

	PoolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	PoolSizes[3].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());

	VkDescriptorPoolCreateInfo PoolCreateInfo = {};
	PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolCreateInfo.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());
	PoolCreateInfo.pPoolSizes = PoolSizes.data();
	PoolCreateInfo.maxSets = static_cast<uint32_t>(m_SwapChainImages.size());

	if (vkCreateDescriptorPool(m_Device, &PoolCreateInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

/** Vulkan Init */void App::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> Layouts(m_SwapChainImages.size(), m_DescriptorSetLayout);
	VkDescriptorSetAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	AllocInfo.descriptorPool = m_DescriptorPool;
	AllocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapChainImages.size());
	AllocInfo.pSetLayouts = Layouts.data();

	m_DescriptorSets.resize(m_SwapChainImages.size());
	if (vkAllocateDescriptorSets(m_Device, &AllocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		VkDescriptorBufferInfo MvpBufferInfo = {};
		MvpBufferInfo.buffer = m_MvpUniformBuffers[i];
		MvpBufferInfo.offset = 0;
		MvpBufferInfo.range = sizeof(MvpUniformBufferObject);

		VkDescriptorBufferInfo LightBufferInfo = {};
		LightBufferInfo.buffer = m_LightUniformBuffers[i];
		LightBufferInfo.offset = 0;
		LightBufferInfo.range = sizeof(LightUniformBufferObject);

		VkDescriptorBufferInfo MaterialBufferInfo = {};
		MaterialBufferInfo.buffer = m_MaterialUniformBuffers[i];
		MaterialBufferInfo.offset = 0;
		MaterialBufferInfo.range = sizeof(MaterialUniformBufferObject);

		VkDescriptorImageInfo ImageInfo = {};
		ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageInfo.imageView = m_AlbedoTextureImageView;
		ImageInfo.sampler = m_AlbedoTextureSamler;

		std::array<VkWriteDescriptorSet, 4> DescriptorWrites = {};
		
		DescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[0].dstSet = m_DescriptorSets[i];
		DescriptorWrites[0].dstBinding = 0;
		DescriptorWrites[0].dstArrayElement = 0;
		DescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrites[0].descriptorCount = 1;
		DescriptorWrites[0].pBufferInfo = &MvpBufferInfo;
		DescriptorWrites[0].pImageInfo = nullptr;
		DescriptorWrites[0].pTexelBufferView = nullptr;

		DescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[1].dstSet = m_DescriptorSets[i];
		DescriptorWrites[1].dstBinding = 1;
		DescriptorWrites[1].dstArrayElement = 0;
		DescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrites[1].descriptorCount = 1;
		DescriptorWrites[1].pBufferInfo = &LightBufferInfo;
		DescriptorWrites[1].pImageInfo = nullptr;
		DescriptorWrites[1].pTexelBufferView = nullptr;

		DescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[2].dstSet = m_DescriptorSets[i];
		DescriptorWrites[2].dstBinding = 2;
		DescriptorWrites[2].dstArrayElement = 0;
		DescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrites[2].descriptorCount = 1;
		DescriptorWrites[2].pBufferInfo = &MaterialBufferInfo;
		DescriptorWrites[2].pImageInfo = nullptr;
		DescriptorWrites[2].pTexelBufferView = nullptr;

		DescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[3].dstSet = m_DescriptorSets[i];
		DescriptorWrites[3].dstBinding = 3;
		DescriptorWrites[3].dstArrayElement = 0;
		DescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		DescriptorWrites[3].descriptorCount = 1;
		DescriptorWrites[3].pBufferInfo = nullptr;
		DescriptorWrites[3].pImageInfo = &ImageInfo;
		DescriptorWrites[3].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(
			m_Device, 
			static_cast<uint32_t>(DescriptorWrites.size()), 
			DescriptorWrites.data(),
			0, 
			nullptr
		);
	}
}

/** Vulkan Init */void App::CreateDrawingCommandBuffers()
{
	m_DrawingCommandBuffers.resize(m_SwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo CmdBufferAllocInfo = {};
	CmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CmdBufferAllocInfo.commandPool = m_CommandPool;
	CmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CmdBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(m_DrawingCommandBuffers.size());

	if (vkAllocateCommandBuffers(m_Device, &CmdBufferAllocInfo, m_DrawingCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}

	for (size_t i = 0; i < m_DrawingCommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo CmdBufferBeginInfo = {};
		CmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		CmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		CmdBufferBeginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(m_DrawingCommandBuffers[i], &CmdBufferBeginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo PassBeginInfo = {};
		PassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		PassBeginInfo.renderPass = m_RenderPass;
		PassBeginInfo.framebuffer = m_SwapChainFramebuffers[i];
		PassBeginInfo.renderArea.offset = { 0, 0 };
		PassBeginInfo.renderArea.extent = m_SwapChainExtent;

		std::array<VkClearValue, 2> ClearColors = {};
		ClearColors[0].color = { 0.1f, 0.2f, 0.3f, 1.0f };
		ClearColors[1].depthStencil = { 1.0f, 0 };

		PassBeginInfo.clearValueCount = static_cast<uint32_t>(ClearColors.size());
		PassBeginInfo.pClearValues = ClearColors.data();

		vkCmdBeginRenderPass(m_DrawingCommandBuffers[i], &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(
			m_DrawingCommandBuffers[i], 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			m_GraphicsPipelines[m_GraphicsPipelineDisplayMode | m_GraphicsPipelineCullMode]
		);

		VkBuffer VertexBuffers[] = { m_VertexBuffer };
		VkDeviceSize Offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_DrawingCommandBuffers[i], 0, 1, VertexBuffers, Offsets);
		vkCmdBindIndexBuffer(m_DrawingCommandBuffers[i], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(
			m_DrawingCommandBuffers[i], 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			m_PipelineLayout, 
			0, 
			1, 
			&m_DescriptorSets[i],
			0, 
			nullptr
		);

		vkCmdDrawIndexed(m_DrawingCommandBuffers[i], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(m_DrawingCommandBuffers[i]);

		if (vkEndCommandBuffer(m_DrawingCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer!");
		}
	}
}

/** Vulkan Init */void App::CreateSyncObjects()
{
	m_ImageAvailableSemaphores.resize(m_MaxFramesInFlights);
	m_RenderFinishedSemaphores.resize(m_MaxFramesInFlights);
	m_InFlightFences.resize(m_MaxFramesInFlights);

	VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
	SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo FenceCreateInfo = {};
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < m_MaxFramesInFlights; i++)
	{
		if (vkCreateSemaphore(m_Device, &SemaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_Device, &SemaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_Device, &FenceCreateInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create semaphores!");
		}
	}
}

/** Callback */VKAPI_ATTR VkBool32 VKAPI_CALL App::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT MessageType,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
	void * pUserData
)
{
	std::cerr << "[Validation layer]" << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

/** Callback */void App::FramebufferResizeCallback(
	GLFWwindow * pWindow, 
	int Width, 
	int Height
)
{
	App * pApp = reinterpret_cast<App*>(glfwGetWindowUserPointer(pWindow));
	if (pApp != nullptr)
	{
		pApp->m_bFramebufferResized = true;
	}
}

/** Callback */void App::MouseButtonCallback(
	GLFWwindow * pWindow, 
	int Button, 
	int Action, 
	int Mods
)
{
	App * pApp = reinterpret_cast<App*>(glfwGetWindowUserPointer(pWindow));
	pApp->m_MouseButton = Button;
	pApp->m_MouseAction = Action;
}

/** Callback */void App::MousePositionCallback(
	GLFWwindow * pWindow, 
	double X, 
	double Y
)
{
	static double PreviousX = X, PreviousY = Y;
	double DeltaX = X - PreviousX;
	double DeltaY = Y - PreviousY;
	PreviousX = X;
	PreviousY = Y;

	App * pApp = reinterpret_cast<App*>(glfwGetWindowUserPointer(pWindow));

	if (pApp->m_MouseButton == GLFW_MOUSE_BUTTON_LEFT && pApp->m_MouseAction != GLFW_RELEASE)
	{
		pApp->m_Camera.UpdateYaw(static_cast<float>(-DeltaX));
		pApp->m_Camera.UpdatePitch(static_cast<float>(DeltaY));
	}

	if (pApp->m_MouseButton == GLFW_MOUSE_BUTTON_RIGHT && pApp->m_MouseAction != GLFW_RELEASE)
	{
		pApp->m_Camera.UpdateTarget(
			static_cast<float>(-DeltaX), 
			static_cast<float>(DeltaY)
		);
	}
}

/** Callback */void App::MouseScrollCallback(
	GLFWwindow * pWindow,
	double OffsetX,
	double OffsetY
)
{
	App * pApp = reinterpret_cast<App*>(glfwGetWindowUserPointer(pWindow));
	pApp->m_Camera.UpdateRadius(static_cast<float>(OffsetY * 0.2));
}

/** Callback */void App::KeyboardCallback(
	GLFWwindow * pWindow,
	int Key,
	int ScanCode,
	int Action,
	int Mods
)
{
	App * pApp = reinterpret_cast<App*>(glfwGetWindowUserPointer(pWindow));
	
	/** [H] : Reset all the status */
	if (Key == GLFW_KEY_H && Action == GLFW_RELEASE)
	{
		pApp->m_Camera.Reset();
		pApp->m_GraphicsPipelineDisplayMode = GRAPHICS_PIPELINE_TYPE_FILL;
		pApp->m_GraphicsPipelineCullMode = GRAPHICS_PIPELINE_TYPE_NONE_CULL;
		pApp->RecreateDrawingCommandBuffer();
	}

	/** [D] : Change display mode */
	if (Key == GLFW_KEY_D && Action == GLFW_RELEASE)
	{
		switch (pApp->m_GraphicsPipelineDisplayMode)
		{
		case GRAPHICS_PIPELINE_TYPE_FILL:
			pApp->m_GraphicsPipelineDisplayMode = GRAPHICS_PIPELINE_TYPE_WIREFRAME;
			break;
		case GRAPHICS_PIPELINE_TYPE_WIREFRAME:
			pApp->m_GraphicsPipelineDisplayMode = GRAPHICS_PIPELINE_TYPE_POINT;
			break;
		case GRAPHICS_PIPELINE_TYPE_POINT:
			pApp->m_GraphicsPipelineDisplayMode = GRAPHICS_PIPELINE_TYPE_FILL;
			break;
		default:
			pApp->m_GraphicsPipelineDisplayMode = GRAPHICS_PIPELINE_TYPE_FILL;
			break;
		}
		pApp->RecreateDrawingCommandBuffer();
	}

	/** [C] : Change cull mode */
	if (Key == GLFW_KEY_C && Action == GLFW_RELEASE)
	{
		switch (pApp->m_GraphicsPipelineCullMode)
		{
		case GRAPHICS_PIPELINE_TYPE_NONE_CULL:
			pApp->m_GraphicsPipelineCullMode = GRAPHICS_PIPELINE_TYPE_FRONT_CULL;
			break;
		case GRAPHICS_PIPELINE_TYPE_FRONT_CULL:
			pApp->m_GraphicsPipelineCullMode = GRAPHICS_PIPELINE_TYPE_BACK_CULL;
			break;
		case GRAPHICS_PIPELINE_TYPE_BACK_CULL:
			pApp->m_GraphicsPipelineCullMode = GRAPHICS_PIPELINE_TYPE_NONE_CULL;
			break;
		default:
			pApp->m_GraphicsPipelineCullMode = GRAPHICS_PIPELINE_TYPE_NONE_CULL;
			break;
		}
		pApp->RecreateDrawingCommandBuffer();
	}
}

/** Helper */std::vector<char> App::ReadFile(
	const std::string & Filename
)
{
	std::ifstream File(Filename, std::ios::ate | std::ios::binary);
	if (!File.is_open())
	{
		throw std::runtime_error("Failed to open file!");
	}
	size_t FileSize = static_cast<size_t>(File.tellg());
	std::vector<char> Buffer(FileSize);
	File.seekg(0);
	File.read(Buffer.data(), Buffer.size());
	File.close();
	return Buffer;
}

size_t App::VertexHash::operator()(const Vertex & Rhs) const
{
	return ((std::hash<glm::vec3>()(Rhs.Position) ^
		(std::hash<glm::vec3>()(Rhs.Color) << 1)) >> 1) ^
		(std::hash<glm::vec2>()(Rhs.TexCoord) << 1);
}

bool App::VertexEqual::operator()(const Vertex & Lhs, const Vertex & Rhs) const
{
	return Lhs.Position == Rhs.Position && 
		Lhs.Color == Rhs.Color && 
		Lhs.TexCoord == Rhs.TexCoord;
}

VkVertexInputBindingDescription App::Vertex::GetBindingDescription()
{
	VkVertexInputBindingDescription BindingDescription = {};
	BindingDescription.binding = 0;
	BindingDescription.stride = sizeof(Vertex);
	BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return BindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> App::Vertex::GetAttributeDescription()
{
	std::array<VkVertexInputAttributeDescription, 4> AttributeDescriptions = {};

	AttributeDescriptions[0].binding = 0;
	AttributeDescriptions[0].location = 0;
	AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	AttributeDescriptions[0].offset = offsetof(Vertex, Position);

	AttributeDescriptions[1].binding = 0;
	AttributeDescriptions[1].location = 1;
	AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	AttributeDescriptions[1].offset = offsetof(Vertex, Color);

	AttributeDescriptions[2].binding = 0;
	AttributeDescriptions[2].location = 2;
	AttributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	AttributeDescriptions[2].offset = offsetof(Vertex, Normal);

	AttributeDescriptions[3].binding = 0;
	AttributeDescriptions[3].location = 3;
	AttributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	AttributeDescriptions[3].offset = offsetof(Vertex, TexCoord);

	return AttributeDescriptions;
}

NAMESPACE_END