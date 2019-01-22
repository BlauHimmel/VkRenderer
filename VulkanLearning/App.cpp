#include "App.hpp"

bool App::QueueFamilyIndices::IsComplete() const
{
	return GraphicsFamily.has_value() && PresentFamily.has_value();
}

void App::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Destroy();
}

void App::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_pWindow = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
}

void App::InitVulkan()
{
	CreateInstance();

	SetupDebugMessenger();

	CreateSurface();

	SelectPhysicalDevice();

	CreateLogicalDevice();

	CreateSwapChain();

	CreateSwapChainImageViews();

	CreateRenderPass();

	CreateGraphicsPipeline();
}

void App::MainLoop()
{
	while (!glfwWindowShouldClose(m_pWindow))
	{
		glfwPollEvents();
	}
}

void App::Destroy()
{
	vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);

	vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

	vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

	for (auto & SwapChainImageView : m_SwapChainImageViews)
	{
		vkDestroyImageView(m_Device, SwapChainImageView, nullptr);
	}
	
	vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
	
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

void App::CreateInstance()
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

void App::SetupDebugMessenger()
{
	if (!m_bEnableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	CreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	CreateInfo.pfnUserCallback = DebugCallback;
	CreateInfo.pUserData = nullptr;

	if (vkCreateDebugUtilsMessengerEXT(m_Instance, &CreateInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}

void App::CreateSurface()
{
	if (glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &m_Surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

void App::SelectPhysicalDevice()
{
	uint32_t DeviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, nullptr);

	if (DeviceCount == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	}

	std::unique_ptr<VkPhysicalDevice[]> PhysicalDevices(new VkPhysicalDevice[DeviceCount]);
	vkEnumeratePhysicalDevices(m_Instance, &DeviceCount, PhysicalDevices.get());

	for (uint32_t i = 0; i < DeviceCount; i++)
	{
		if (IsPhysicalDeviceSuitable(PhysicalDevices[i], m_Surface))
		{
			m_PhysicalDevice = PhysicalDevices[i];
			break;
		}
	}

	if (m_PhysicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Failed to find a support GPU!");
	}
}

void App::CreateLogicalDevice()
{
	QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice);
	float QueuePriority = 1.0f;

	std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
	std::set<uint32_t> UniqueQueueFamilies = { Indices.GraphicsFamily.value(), Indices.PresentFamily.value() };

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

/** Helper */bool App::IsPhysicalDeviceSuitable(VkPhysicalDevice Device, VkSurfaceKHR Surface) const
{
	QueueFamilyIndices Indices = FindQueueFamilies(Device);
	bool bExtensionsSupported = CheckPhysicalDeviceExtensionsSupport(Device);
	bool bSwapChainAdequate = false;
	if (bExtensionsSupported)
	{
		SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(Device, Surface);
		bSwapChainAdequate = !SwapChainSupport.Formats.empty() && !SwapChainSupport.PresentModes.empty();
	}
	return Indices.IsComplete() && bExtensionsSupported && bSwapChainAdequate;
}

/** Helper */App::QueueFamilyIndices App::FindQueueFamilies(VkPhysicalDevice Device) const
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

/** Helper */bool App::CheckPhysicalDeviceExtensionsSupport(VkPhysicalDevice Device) const
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

void App::CreateSwapChain()
{
	SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(m_PhysicalDevice, m_Surface);
	VkSurfaceFormatKHR SurfaceFormat = ChooseSwapSurfaceFormat(SwapChainSupport.Formats);
	VkPresentModeKHR PresentMode = ChooseSwapPresentMode(SwapChainSupport.PresentModes);
	VkExtent2D Extent = ChooseSwapExtent(SwapChainSupport.Capabilities);

	uint32_t ImageCount = SwapChainSupport.Capabilities.minImageCount + 1;
	if (SwapChainSupport.Capabilities.maxImageCount > 0 && ImageCount > SwapChainSupport.Capabilities.maxImageCount)
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
	uint32_t QueueFamilyIndices[] = { Indices.GraphicsFamily.value(), Indices.PresentFamily.value() };

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

/** Helper */App::SwapChainSupportDetails App::QuerySwapChainSupport(VkPhysicalDevice Device, VkSurfaceKHR Surface) const
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
		vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, Details.PresentModes.data());
	}

	return Details;
}

/** Helper */VkSurfaceFormatKHR App::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats) const
{
	if (AvailableFormats.size() == 1 && AvailableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto & AvailableFormat : AvailableFormats)
	{
		if (AvailableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && AvailableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return AvailableFormat;
		}
	}

	return AvailableFormats[0];
}

/** Helper */VkPresentModeKHR App::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes) const
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

/** Helper */VkExtent2D App::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR & Capabilities) const
{
	if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return Capabilities.currentExtent;
	}
	else
	{
		VkExtent2D ActualExtent = { m_Width, m_Height };
		ActualExtent.width = std::max(Capabilities.minImageExtent.width, std::min(Capabilities.maxImageExtent.width, ActualExtent.width));
		ActualExtent.height = std::max(Capabilities.minImageExtent.height, std::min(Capabilities.maxImageExtent.height, ActualExtent.height));
		return ActualExtent;
	}
}

void App::CreateSwapChainImageViews()
{
	m_SwapChainImageViews.resize(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		VkImageViewCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		CreateInfo.image = m_SwapChainImages[i];
		CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		CreateInfo.format = m_SwapChainImageFormat;
		CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		CreateInfo.subresourceRange.baseMipLevel = 0;
		CreateInfo.subresourceRange.levelCount = 1;
		CreateInfo.subresourceRange.baseArrayLayer = 0;
		CreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_Device, &CreateInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image views!");
		}
	}
}

void App::CreateRenderPass()
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

	VkAttachmentReference ColorAttachmentRef = {};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;

	VkRenderPassCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	CreateInfo.attachmentCount = 1;
	CreateInfo.pAttachments = &ColorAttachment;
	CreateInfo.subpassCount = 1;
	CreateInfo.pSubpasses = &Subpass;

	if (vkCreateRenderPass(m_Device, &CreateInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass!");
	}
}

void App::CreateGraphicsPipeline()
{
	// Shader modules
	auto VertShaderCode = ReadFile("Shaders/Triangle.vert.spv");
	auto FragShaderCode = ReadFile("Shaders/Triangle.frag.spv");

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

	VkPipelineShaderStageCreateInfo ShaderStageCreateInfos[] = { VertShaderStageCreateInfo, FragShaderStageCreateInfo };

	// Vertex input
	VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {};
	VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	VertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	VertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	VertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

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
	RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	// TODO : Use it later...

	// Color blending
	VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
	ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
	PipelineLayoutCreateInfo.setLayoutCount = 0;
	PipelineLayoutCreateInfo.pSetLayouts = nullptr;
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
	GraphicsPipelineCreateInfo.pDepthStencilState = nullptr;
	GraphicsPipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
	GraphicsPipelineCreateInfo.pDynamicState = nullptr;
	GraphicsPipelineCreateInfo.layout = m_PipelineLayout;
	GraphicsPipelineCreateInfo.renderPass = m_RenderPass;
	GraphicsPipelineCreateInfo.subpass = 0;
	GraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	GraphicsPipelineCreateInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(m_Device, VertShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, FragShaderModule, nullptr);
}

/** Helper */VkShaderModule App::CreateShaderModule(VkDevice Device, const std::vector<char>& ShaderCode)
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

/** Callback */VKAPI_ATTR VkBool32 VKAPI_CALL App::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT MessageType,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
	void * pUserData
)
{
	std::cerr << "Validation layer : " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
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
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
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
}

/** Helper */std::vector<char> App::ReadFile(const std::string & Filename)
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

