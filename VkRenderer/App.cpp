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
#include <algorithm>
#include <fstream>
#include <limits>
#include <iostream>
#include <memory>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <unordered_map>

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

	CreateDepthResource();

	CreateFramebuffers();

	CreateTextureImage();

	CreateTextureImageView();

	CreateTextureSampler();

	LoadObjModel();

	CreateVertexBuffer();

	CreateIndexBuffer();

	CreateUniformBuffer();

	CreateDescriptorPool();

	CreateDescriptorSets();

	CreateCommandBuffers();

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

			char Buffer[256];
			sprintf_s(
				Buffer, "%s [%s] [Vertex : %d Facet : %d] Fps: %d", 
				m_Title.c_str(), 
				m_GpuName.c_str(),
				static_cast<int32_t>(m_VertexNum), 
				static_cast<int32_t>(m_FacetNum),
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
	SubmitInfo.pCommandBuffers = &m_CommandBuffers[ImageIndex];

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
		vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
		vkFreeMemory(m_Device, m_UniformBufferMemories[i], nullptr);
	}

	vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
	vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

	vkDestroySampler(m_Device, m_TextureSamler, nullptr);

	vkDestroyImageView(m_Device, m_TextureImageView, nullptr);

	vkDestroyImage(m_Device, m_TextureImage, nullptr);
	vkFreeMemory(m_Device, m_TextureImageMemory, nullptr);

	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
	
	vkDestroyDevice(m_Device, nullptr);
	
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	
	if (m_bEnableValidationLayers)
	{
		vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}
	
	vkDestroyInstance(m_Instance, nullptr);

	glfwDestroyWindow(m_pWindow);
	
	glfwTerminate();
}

/** App Helper */void App::UpdateUniformBuffer(
	uint32_t CurrentImage
)
{
	static auto StartTime = std::chrono::high_resolution_clock::now();
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	float DeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(CurrentTime - StartTime).count();

	UniformBufferObject Transformation = {};
	Transformation.Model = glm::rotate(
		glm::mat4(1.0f), 
		DeltaTime * glm::radians(90.0f), 
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	Transformation.View = glm::lookAt(
		glm::vec3(2.0f, 2.0f, 2.0f), 
		glm::vec3(0.0f, 0.0f, 0.0f), 
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	Transformation.Projection = glm::perspective(
		glm::radians(45.0f), 
		static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height), 
		0.1f, 
		10.0f
	);
	/** GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted */
	Transformation.Projection[1][1] *= -1.0f;

	void * pData = nullptr;
	vkMapMemory(m_Device, m_UniformBufferMemories[CurrentImage], 0, sizeof(Transformation), 0, &pData);
	memcpy(pData, &Transformation, sizeof(Transformation));
	vkUnmapMemory(m_Device, m_UniformBufferMemories[CurrentImage]);
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

	CreateDepthResource();

	CreateFramebuffers();

	CreateCommandBuffers();
}

/** App Helper */void App::DestroySwapChainAndRelevantObject()
{
	vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
	vkDestroyImage(m_Device, m_DepthImage, nullptr);
	vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

	for (auto & Framebuffer : m_SwapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_Device, Framebuffer, nullptr);
	}

	vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);

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
		static_cast<uint32_t>(m_CommandBuffers.size()), 
		m_CommandBuffers.data()
	);
}

/** Vulkan Init */void App::CreateInstance()
{
	if (m_bEnableValidationLayers && !CheckValidationLayerSupport())
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

	auto Extensions = GetRequiredExtensions();

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

	if (vkCreateDebugUtilsMessengerEXT(m_Instance, &CreateInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
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

	uint32_t MaxMemoryAllocationCount = 0;
	for (uint32_t i = 0; i < DeviceCount; i++)
	{
		if (IsPhysicalDeviceSuitable(PhysicalDevices[i], m_Surface))
		{
			VkPhysicalDeviceProperties PhysicalDeviceProperties;
			vkGetPhysicalDeviceProperties(PhysicalDevices[i], &PhysicalDeviceProperties);

			if (PhysicalDeviceProperties.limits.maxMemoryAllocationCount > MaxMemoryAllocationCount)
			{
				m_PhysicalDevice = PhysicalDevices[i];
				m_GpuName = PhysicalDeviceProperties.deviceName;
				MaxMemoryAllocationCount = PhysicalDeviceProperties.limits.maxMemoryAllocationCount;
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
	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice);
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
	VkExtent2D Extent = ChooseSwapExtent(m_pWindow, SwapChainSupport.Capabilities);

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

	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice);
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
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_SwapChainImageViews[i]
		);
	}
}

/** Vulkan Init */void App::CreateRenderPass()
{
	VkAttachmentDescription ColorAttachment = {};
	ColorAttachment.format = m_SwapChainImageFormat;
	ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription DepthAttachment = {};
	DepthAttachment.format = FindDepthFormat();
	DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference ColorAttachmentRef = {};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference DepthAttachmentRef = {};
	DepthAttachmentRef.attachment = 1;
	DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;
	Subpass.pDepthStencilAttachment = &DepthAttachmentRef;

	std::array<VkAttachmentDescription, 2> Attachments = 
	{
		ColorAttachment, DepthAttachment
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
	VkDescriptorSetLayoutBinding UboLayoutBinding = {};
	UboLayoutBinding.binding = 0;
	UboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	UboLayoutBinding.descriptorCount = 1;
	UboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	UboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding SamplerLayoutBinding = {};
	SamplerLayoutBinding.binding = 1;
	SamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	SamplerLayoutBinding.descriptorCount = 1;
	SamplerLayoutBinding.pImmutableSamplers = nullptr;
	SamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> Bindings =
	{
		UboLayoutBinding,
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
	RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	RasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	RasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	RasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	RasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	// Multisampling
	VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {};
	MultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	MultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	MultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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

	if (vkCreateGraphicsPipelines(
		m_Device,
		VK_NULL_HANDLE, 
		1, 
		&GraphicsPipelineCreateInfo,
		nullptr, 
		&m_GraphicsPipeline
	) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(m_Device, VertShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, FragShaderModule, nullptr);
}

/** Vulkan Init */void App::CreateCommandPool()
{
	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice);

	VkCommandPoolCreateInfo CmdPoolCreateInfo = {};
	CmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	CmdPoolCreateInfo.queueFamilyIndex = Indices.GraphicsFamily.value();
	CmdPoolCreateInfo.flags = 0;

	if (vkCreateCommandPool(m_Device, &CmdPoolCreateInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

/** Vulkan Init */void App::CreateDepthResource()
{
	VkFormat DepthFormat = FindDepthFormat();
	CreateImage(
		m_Device,
		m_SwapChainExtent.width,
		m_SwapChainExtent.height,
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
		VK_IMAGE_ASPECT_DEPTH_BIT,
		m_DepthImageView
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_DepthImage,
		DepthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);
}

/** Vulkan Init */void App::CreateFramebuffers()
{
	m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

	for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
	{
		std::array<VkImageView, 2> Attachments =
		{
			m_SwapChainImageViews[i],
			m_DepthImageView
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

/** Vulkan Init */void App::CreateTextureImage()
{
	int TexWidth = -1, TexHeight = -1, TexChannels = -1;
	stbi_uc * pPixels = stbi_load(
		m_TexturePath.c_str(),
		&TexWidth, 
		&TexHeight, 
		&TexChannels,
		STBI_rgb_alpha
	);
	VkDeviceSize ImageSize = TexWidth * TexHeight * 4;

	if (pPixels == nullptr)
	{
		throw std::runtime_error("Failed to load texture image!");
	}

	VkBuffer StagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;

	CreateBuffer(
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
		m_Device,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight),
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_TextureImage,
		m_TextureImageMemory
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_TextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	CopyBufferToImage(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		StagingBuffer,
		m_TextureImage,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight)
	);

	TransitionImageLayout(
		m_Device,
		m_GraphicsQueue,
		m_CommandPool,
		m_TextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	vkDestroyBuffer(m_Device, StagingBuffer, nullptr);
	vkFreeMemory(m_Device, StagingBufferMemory, nullptr);
}

/** Vulkan Init */void App::CreateTextureImageView()
{
	CreateImageView(
		m_Device, 
		m_TextureImage, 
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_ASPECT_COLOR_BIT,
		m_TextureImageView
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
	CreateInfo.maxLod = 0.0f;

	if (vkCreateSampler(m_Device, &CreateInfo, nullptr, &m_TextureSamler) != VK_SUCCESS)
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

			Vertex.Color = { 1.0f, 1.0f, 1.0f };

			Vertex.TexCoord =
			{
				Attrib.texcoords[2 * Index.texcoord_index + 0],
				Attrib.texcoords[2 * Index.texcoord_index + 1]
			};

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

/** Vulkan Init */void App::CreateUniformBuffer()
{
	VkDeviceSize BufferSize = sizeof(UniformBufferObject);

	m_UniformBuffers.resize(m_SwapChainImages.size());
	m_UniformBufferMemories.resize(m_SwapChainImages.size());

	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		CreateBuffer(
			m_Device,
			BufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_UniformBuffers[i],
			m_UniformBufferMemories[i]
		);
	}
}

/** Vulkan Init */void App::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> PoolSizes = {};
	
	PoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	PoolSizes[0].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());
	
	PoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	PoolSizes[1].descriptorCount = static_cast<uint32_t>(m_SwapChainImages.size());

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
		VkDescriptorBufferInfo BufferInfo = {};
		BufferInfo.buffer = m_UniformBuffers[i];
		BufferInfo.offset = 0;
		BufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo ImageInfo = {};
		ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageInfo.imageView = m_TextureImageView;
		ImageInfo.sampler = m_TextureSamler;

		std::array<VkWriteDescriptorSet, 2> DescriptorWrites = {};
		
		DescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[0].dstSet = m_DescriptorSets[i];
		DescriptorWrites[0].dstBinding = 0;
		DescriptorWrites[0].dstArrayElement = 0;
		DescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrites[0].descriptorCount = 1;
		DescriptorWrites[0].pBufferInfo = &BufferInfo;
		DescriptorWrites[0].pImageInfo = nullptr;
		DescriptorWrites[0].pTexelBufferView = nullptr;

		DescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[1].dstSet = m_DescriptorSets[i];
		DescriptorWrites[1].dstBinding = 1;
		DescriptorWrites[1].dstArrayElement = 0;
		DescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		DescriptorWrites[1].descriptorCount = 1;
		DescriptorWrites[1].pBufferInfo = nullptr;
		DescriptorWrites[1].pImageInfo = &ImageInfo;
		DescriptorWrites[1].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(
			m_Device, 
			static_cast<uint32_t>(DescriptorWrites.size()), 
			DescriptorWrites.data(),
			0, 
			nullptr
		);
	}
}

/** Vulkan Init */void App::CreateCommandBuffers()
{
	m_CommandBuffers.resize(m_SwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo CmdBufferAllocInfo = {};
	CmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CmdBufferAllocInfo.commandPool = m_CommandPool;
	CmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CmdBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	if (vkAllocateCommandBuffers(m_Device, &CmdBufferAllocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}

	for (size_t i = 0; i < m_CommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo CmdBufferBeginInfo = {};
		CmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		CmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		CmdBufferBeginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(m_CommandBuffers[i], &CmdBufferBeginInfo) != VK_SUCCESS)
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

		vkCmdBeginRenderPass(m_CommandBuffers[i], &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		vkCmdBindPipeline(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
		
		VkBuffer VertexBuffers[] = { m_VertexBuffer };
		VkDeviceSize Offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_CommandBuffers[i], 0, 1, VertexBuffers, Offsets);
		vkCmdBindIndexBuffer(m_CommandBuffers[i], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(
			m_CommandBuffers[i], 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			m_PipelineLayout, 
			0, 
			1, 
			&m_DescriptorSets[i],
			0, 
			nullptr
		);

		vkCmdDrawIndexed(m_CommandBuffers[i], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(m_CommandBuffers[i]);

		if (vkEndCommandBuffer(m_CommandBuffers[i]) != VK_SUCCESS)
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

/** Helper Struct */bool App::QueueFamilyIndices::IsComplete() const
{
	return GraphicsFamily.has_value() && PresentFamily.has_value();
}

/** Helper */bool App::CheckValidationLayerSupport() const
{
	uint32_t LayerCount = 0;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
	std::unique_ptr<VkLayerProperties[]> AvailableLayers(new VkLayerProperties[LayerCount]);
	vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.get());

	for (const auto & LayerName : m_ValidationLayers)
	{
		bool bLayerFound = false;
		for (uint32_t i = 0; i < LayerCount; i++)
		{
			if (strcmp(LayerName, AvailableLayers[i].layerName) == 0)
			{
				bLayerFound = true;
				break;
			}
		}

		if (!bLayerFound)
		{
			return false;
		}
	}

	return true;
}

/** Helper */std::vector<const char *> App::GetRequiredExtensions() const
{
	uint32_t GlfwExtensionCount = 0;
	const char ** ppGlfwExtensions = glfwGetRequiredInstanceExtensions(&GlfwExtensionCount);

	std::vector<const char *> Extensions(ppGlfwExtensions, ppGlfwExtensions + GlfwExtensionCount);

	if (m_bEnableValidationLayers)
	{
		Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return Extensions;
}

/** Helper */bool App::IsPhysicalDeviceSuitable(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
) const
{
	QueueFamilyIndices Indices = FindQueueFamilies(Device);
	bool bExtensionsSupported = CheckPhysicalDeviceExtensionsSupport(Device);
	bool bSwapChainAdequate = false;

	if (bExtensionsSupported)
	{
		SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(Device, Surface);
		bSwapChainAdequate = !SwapChainSupport.Formats.empty() && !SwapChainSupport.PresentModes.empty();
	}

	VkPhysicalDeviceFeatures SupportedFeatures;
	vkGetPhysicalDeviceFeatures(Device, &SupportedFeatures);

	return Indices.IsComplete() &&
		bExtensionsSupported &&
		bSwapChainAdequate &&
		SupportedFeatures.samplerAnisotropy;
}


/** Helper */App::QueueFamilyIndices App::FindQueueFamilies(
	VkPhysicalDevice Device
) const
{
	QueueFamilyIndices Indices;

	uint32_t QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);
	std::unique_ptr<VkQueueFamilyProperties[]> QueueFamilies(new VkQueueFamilyProperties[QueueFamilyCount]);
	vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilies.get());

	for (uint32_t i = 0; i < QueueFamilyCount; i++)
	{
		if (QueueFamilies[i].queueCount > 0 && QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			Indices.GraphicsFamily = i;
		}

		VkBool32 bPresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, m_Surface, &bPresentSupport);
		if (QueueFamilies[i].queueCount > 0 && bPresentSupport)
		{
			Indices.PresentFamily = i;
		}

		if (Indices.IsComplete())
		{
			break;
		}
	}

	return Indices;
}

/** Helper */bool App::CheckPhysicalDeviceExtensionsSupport(
	VkPhysicalDevice Device
) const
{
	uint32_t ExtensionCount = 0;
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, nullptr);
	std::unique_ptr<VkExtensionProperties[]> AvailableExtensions(new VkExtensionProperties[ExtensionCount]);
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, AvailableExtensions.get());

	std::set<std::string> RequiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
	for (uint32_t i = 0; i < ExtensionCount; i++)
	{
		RequiredExtensions.erase(AvailableExtensions[i].extensionName);
	}

	return RequiredExtensions.empty();
}

/** Helper */App::SwapChainSupportDetails App::QuerySwapChainSupport(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
) const
{
	SwapChainSupportDetails Details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Surface, &Details.Capabilities);

	uint32_t FormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, nullptr);
	if (FormatCount != 0)
	{
		Details.Formats.resize(FormatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, Details.Formats.data());
	}

	uint32_t PresentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, nullptr);
	if (PresentModeCount != 0)
	{
		Details.PresentModes.resize(PresentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			Device,
			Surface,
			&PresentModeCount,
			Details.PresentModes.data()
		);
	}

	return Details;
}

/** Helper */VkSurfaceFormatKHR App::ChooseSwapSurfaceFormat(
	const std::vector<VkSurfaceFormatKHR>& AvailableFormats
) const
{
	if (AvailableFormats.size() == 1 && AvailableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto & AvailableFormat : AvailableFormats)
	{
		if (AvailableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
			AvailableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return AvailableFormat;
		}
	}

	return AvailableFormats[0];
}

/** Helper */VkPresentModeKHR App::ChooseSwapPresentMode(
	const std::vector<VkPresentModeKHR>& AvailablePresentModes
) const
{
	VkPresentModeKHR BestMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto & AvailablePresentMode : AvailablePresentModes)
	{
		if (AvailablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return AvailablePresentMode;
		}
		else if (AvailablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			BestMode = AvailablePresentMode;
		}
	}

	return BestMode;
}

/** Helper */VkExtent2D App::ChooseSwapExtent(
	GLFWwindow * pWindow,
	const VkSurfaceCapabilitiesKHR & Capabilities
) const
{
	if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return Capabilities.currentExtent;
	}
	else
	{
		int Width, Height;
		glfwGetFramebufferSize(pWindow, &Width, &Height);

		VkExtent2D ActualExtent =
		{
			static_cast<uint32_t>(m_InitWidth),
			static_cast<uint32_t>(m_InitHeight)
		};
		ActualExtent.width = std::max(
			Capabilities.minImageExtent.width,
			std::min(Capabilities.maxImageExtent.width, ActualExtent.width)
		);
		ActualExtent.height = std::max(
			Capabilities.minImageExtent.height,
			std::min(Capabilities.maxImageExtent.height, ActualExtent.height)
		);
		return ActualExtent;
	}
}

/** Helper */void App::CreateImageView(
	VkDevice Device,
	VkImage Image,
	VkFormat Format,
	VkImageAspectFlags AspectFlags,
	VkImageView & ImageView
)
{
	VkImageViewCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	CreateInfo.image = Image;
	CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	CreateInfo.format = Format;
	CreateInfo.subresourceRange.aspectMask = AspectFlags;
	CreateInfo.subresourceRange.baseMipLevel = 0;
	CreateInfo.subresourceRange.levelCount = 1;
	CreateInfo.subresourceRange.baseArrayLayer = 0;
	CreateInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(Device, &CreateInfo, nullptr, &ImageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture image view!");
	}
}

/** Helper */VkShaderModule App::CreateShaderModule(
	VkDevice Device,
	const std::vector<char>& ShaderCode
)
{
	VkShaderModuleCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfo.codeSize = ShaderCode.size();
	CreateInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderCode.data());

	VkShaderModule ShaderModule;
	if (vkCreateShaderModule(Device, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}
	return ShaderModule;
}

/** Helper */VkFormat App::FindSupportedFormat(
	VkPhysicalDevice Device,
	const std::vector<VkFormat>& Candidates,
	VkImageTiling Tiling,
	VkFormatFeatureFlags Features
) const
{
	for (VkFormat Format : Candidates)
	{
		VkFormatProperties Props;
		vkGetPhysicalDeviceFormatProperties(Device, Format, &Props);
		if (Tiling == VK_IMAGE_TILING_LINEAR && (Props.linearTilingFeatures & Features) == Features)
		{
			return Format;
		}
		else if (Tiling == VK_IMAGE_TILING_OPTIMAL && (Props.optimalTilingFeatures & Features) == Features)
		{
			return Format;
		}
	}

	throw std::runtime_error("Failed to find supported format!");
}

/** Helper */VkFormat App::FindDepthFormat()
{
	return FindSupportedFormat(
		m_PhysicalDevice,
		{
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

/** Helper */bool App::HasStencilComponent(
	VkFormat Format
) const
{
	return Format == VK_FORMAT_D32_SFLOAT_S8_UINT || Format == VK_FORMAT_D24_UNORM_S8_UINT;
}

/** Helper */uint32_t App::FindMemoryType(
	uint32_t TypeFilter,
	VkMemoryPropertyFlags Properties
)
{
	VkPhysicalDeviceMemoryProperties MemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &MemoryProperties);

	for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++)
	{
		if (TypeFilter & (1 << i) && (MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

/** Helper */void App::CreateBuffer(
	VkDevice Device,
	VkDeviceSize Size,
	VkBufferUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	VkBuffer & Buffer,
	VkDeviceMemory & BufferMemory
)
{
	VkBufferCreateInfo BufferCreateInfo = {};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = Size;
	BufferCreateInfo.usage = Usage;
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(Device, &BufferCreateInfo, nullptr, &Buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer!");
	}

	VkMemoryRequirements MemoryRequirements;
	vkGetBufferMemoryRequirements(Device, Buffer, &MemoryRequirements);

	VkMemoryAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemoryRequirements.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(MemoryRequirements.memoryTypeBits, Properties);

	if (vkAllocateMemory(Device, &AllocInfo, nullptr, &BufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory!");
	}

	vkBindBufferMemory(Device, Buffer, BufferMemory, 0);
}

/** Helper */void App::CopyBuffer(
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	VkBuffer SrcBuffer,
	VkBuffer DstBuffer,
	VkDeviceSize Size
)
{
	VkCommandBuffer CommandBuffer = BeginSingleTimeCommands(Device, CommandPool);

	VkBufferCopy CopyRegion = {};
	CopyRegion.srcOffset = 0;
	CopyRegion.dstOffset = 0;
	CopyRegion.size = Size;
	vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &CopyRegion);

	EndSingleTimeCommands(Device, Queue, CommandPool, CommandBuffer);
}

/** Helper */void App::CreateImage(
	VkDevice Device,
	uint32_t Width,
	uint32_t Height,
	VkFormat Format,
	VkImageTiling Tiling,
	VkImageUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	VkImage & Image,
	VkDeviceMemory & ImageMemory
)
{
	VkImageCreateInfo ImageCreateInfo = {};
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageCreateInfo.extent.width = Width;
	ImageCreateInfo.extent.height = Height;
	ImageCreateInfo.extent.depth = 1;
	ImageCreateInfo.mipLevels = 1;
	ImageCreateInfo.arrayLayers = 1;
	ImageCreateInfo.format = Format;
	ImageCreateInfo.tiling = Tiling;
	ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageCreateInfo.usage = Usage;
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(Device, &ImageCreateInfo, nullptr, &Image) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture image!");
	}

	VkMemoryRequirements MemRequirements;
	vkGetImageMemoryRequirements(Device, Image, &MemRequirements);

	VkMemoryAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemRequirements.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(MemRequirements.memoryTypeBits, Properties);

	if (vkAllocateMemory(Device, &AllocInfo, nullptr, &ImageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture image memory!");
	}

	vkBindImageMemory(Device, Image, ImageMemory, 0);
}

/** Helper */VkCommandBuffer App::BeginSingleTimeCommands(
	VkDevice Device,
	VkCommandPool CommandPool
)
{
	VkCommandBufferAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocInfo.commandPool = CommandPool;
	AllocInfo.commandBufferCount = 1;

	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(Device, &AllocInfo, &CommandBuffer);

	VkCommandBufferBeginInfo CmdBeginInfo = {};
	CmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(CommandBuffer, &CmdBeginInfo);

	return CommandBuffer;
}

/** Helper */void App::EndSingleTimeCommands(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkCommandBuffer CommandBuffer
)
{
	vkEndCommandBuffer(CommandBuffer);

	VkSubmitInfo SubmitInfo = {};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CommandBuffer;

	vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(Queue);

	vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
}

/** Helper */void App::TransitionImageLayout(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkImage Image,
	VkFormat Format,
	VkImageLayout OldLayout,
	VkImageLayout NewLayout
)
{
	VkCommandBuffer CommandBuffer = BeginSingleTimeCommands(Device, CommandPool);

	VkImageMemoryBarrier Barrier = {};
	Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	Barrier.oldLayout = OldLayout;
	Barrier.newLayout = NewLayout;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.image = Image;

	if (NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (HasStencilComponent(Format))
		{
			Barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	Barrier.subresourceRange.baseMipLevel = 0;
	Barrier.subresourceRange.levelCount = 1;
	Barrier.subresourceRange.baseArrayLayer = 0;
	Barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags SrcStage;
	VkPipelineStageFlags DstStage;

	if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		Barrier.srcAccessMask = 0;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		SrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		DstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		Barrier.srcAccessMask = 0;
		Barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		DstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::runtime_error("Unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		CommandBuffer,
		SrcStage,
		DstStage,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&Barrier
	);

	EndSingleTimeCommands(Device, Queue, CommandPool, CommandBuffer);
}

/** Helper */void App::CopyBufferToImage(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkBuffer SrcBuffer,
	VkImage DstImage,
	uint32_t Width,
	uint32_t Height
)
{
	VkCommandBuffer CommandBuffer = BeginSingleTimeCommands(Device, CommandPool);

	VkBufferImageCopy Region = {};
	Region.bufferOffset = 0;
	Region.bufferRowLength = 0;
	Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset = { 0, 0, 0 };
	Region.imageExtent = { Width, Height, 1 };

	vkCmdCopyBufferToImage(
		CommandBuffer,
		SrcBuffer,
		DstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&Region
	);

	EndSingleTimeCommands(Device, Queue, CommandPool, CommandBuffer);
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

/** Loader */VkResult App::vkCreateDebugUtilsMessengerEXT(
	VkInstance Instance,
	const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
	const VkAllocationCallbacks * pAllocator,
	VkDebugUtilsMessengerEXT * pDebugMessenger
)
{
	static auto Func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
	if (Func != nullptr)
	{
		return Func(Instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	
	throw std::runtime_error("Function vkCreateDebugUtilsMessengerEXT not found!");
}

/** Loader */void App::vkDestroyDebugUtilsMessengerEXT(
	VkInstance Instance,
	VkDebugUtilsMessengerEXT DebugMessenger,
	const VkAllocationCallbacks * pAllocator
)
{
	static auto Func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT");
	if (Func != nullptr)
	{
		Func(Instance, DebugMessenger, pAllocator);
	}

	throw std::runtime_error("Function vkCreateDebugUtilsMessengerEXT not found!");
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
