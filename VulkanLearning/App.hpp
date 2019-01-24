#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>

class App
{
protected:
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;

		bool IsComplete() const;
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

public:
	void Run();

protected:
	void InitWindow();
	void InitVulkan();
	void MainLoop();
	void Draw();
	void Destroy();

	void UpdateUniformBuffer(
		uint32_t CurrentImage
	);

	/** Recreate the swapchain and all the objects depend on it. */
	void RecreateSwapChainAndRelevantObject();

	/** Destroy the objects that are to be recreated in the RecreateSwapChain(). */
	void DestroySwapChainAndRelevantObject();

protected:
	void CreateInstance();
	/** Helper */bool CheckValidationLayerSupport() const;
	/** Helper */std::vector<const char *> GetRequiredExtensions() const;

	void SetupDebugMessenger();

	void CreateSurface();

	void SelectPhysicalDevice();

	void CreateLogicalDevice();
	/** Helper */bool IsPhysicalDeviceSuitable(
		VkPhysicalDevice Device, 
		VkSurfaceKHR Surface
	) const;
	/** Helper */QueueFamilyIndices FindQueueFamilies(
		VkPhysicalDevice Device
	) const;
	/** Helper */bool CheckPhysicalDeviceExtensionsSupport(
		VkPhysicalDevice Device
	) const;
	/** Helper */SwapChainSupportDetails QuerySwapChainSupport(
		VkPhysicalDevice Device, 
		VkSurfaceKHR Surface
	) const;

	void CreateSwapChain();
	/** Helper */VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
		const std::vector<VkSurfaceFormatKHR> & AvailableFormats
	) const;
	/** Helper */VkPresentModeKHR ChooseSwapPresentMode(
		const std::vector<VkPresentModeKHR> & AvailablePresentModes
	) const;
	/** Helper */VkExtent2D ChooseSwapExtent(
		GLFWwindow * pWindow, 
		const VkSurfaceCapabilitiesKHR & Capabilities
	) const;
	
	void CreateSwapChainImageViews();

	void CreateRenderPass();

	void CreateDescriptorSetLayout();

	void CreateGraphicsPipeline();
	/** Helper */VkShaderModule CreateShaderModule(
		VkDevice Device, 
		const std::vector<char> & ShaderCode
	);

	void CreateFramebuffers();

	void CreateCommandPool();

	void CreateTextureBuffer();
	/** Helper */uint32_t FindMemoryType(
		uint32_t TypeFilter, 
		VkMemoryPropertyFlags Properties
	);
	/** Helper */void CreateBuffer(
		VkDevice Device,
		VkDeviceSize Size,
		VkBufferUsageFlags Usage,
		VkMemoryPropertyFlags Properties,
		VkBuffer & Buffer,
		VkDeviceMemory & BufferMemory
	);
	/** Helper */void CopyBuffer(
		VkDevice Device,
		VkCommandPool CommandPool,
		VkQueue Queue,
		VkBuffer SrcBuffer,
		VkBuffer DstBuffer,
		VkDeviceSize Size
	);
	/** Helper */void CreateImage(
		VkDevice Device,
		uint32_t Width,
		uint32_t Height,
		VkFormat Format,
		VkImageTiling Tiling,
		VkImageUsageFlags Usage,
		VkMemoryPropertyFlags Properties,
		VkImage & Image,
		VkDeviceMemory & ImageMemory
	);
	/** Helper */VkCommandBuffer BeginSingleTimeCommands(
		VkDevice Device, 
		VkCommandPool CommandPool
	);
	/** Helper */void EndSingleTimeCommands(
		VkDevice Device, 
		VkQueue Queue, 
		VkCommandPool CommandPool, 
		VkCommandBuffer CommandBuffer
	);
	/** Helper */void TransitionImageLayout(
		VkDevice Device,
		VkQueue Queue,
		VkCommandPool CommandPool,
		VkImage Image, 
		VkFormat Format,
		VkImageLayout OldLayout,
		VkImageLayout NewLayout
	);
	/** Helper */void CopyBufferToImage(
		VkDevice Device,
		VkQueue Queue,
		VkCommandPool CommandPool,
		VkBuffer SrcBuffer,
		VkImage DstImage,
		uint32_t Width,
		uint32_t Height
	);

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	void CreateUniformBuffer();

	void CreateDescriptorPool();

	void CreateDescriptorSets();

	void CreateCommandBuffers();

	void CreateSyncObjects();

protected:
	/** Callback */static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT MessageType,
		const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
		void * pUserData
	);

	/** Callback */static void FramebufferResizeCallback(
		GLFWwindow * pWindow, 
		int Width, 
		int Height
	);

	/** Loader */static VkResult vkCreateDebugUtilsMessengerEXT(
		VkInstance Instance,
		const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
		const VkAllocationCallbacks * pAllocator,
		VkDebugUtilsMessengerEXT * pDebugMessenger
	);

	/** Loader */static void vkDestroyDebugUtilsMessengerEXT(
		VkInstance Instance,
		VkDebugUtilsMessengerEXT DebugMessenger,
		const VkAllocationCallbacks * pAllocator
	);

	/** Helper */static std::vector<char> ReadFile(
		const std::string & Filename
	);

protected:
	GLFWwindow * m_pWindow = nullptr;
	uint32_t m_InitWidth = 800;
	uint32_t m_InitHeight = 600;
	std::string m_Title = "Vulkan";
	std::string m_AppName = "VulkanApp";
	std::string m_EngineName = "VulkanEngine";
	bool m_bFramebufferResized = false;
	double m_FPS = 0.0f;

protected:
#ifdef NDEBUG
	const bool m_bEnableValidationLayers = false;
#else
	const bool m_bEnableValidationLayers = true;
#endif
	const std::vector<const char *> m_ValidationLayers =
	{
		"VK_LAYER_LUNARG_standard_validation"
	};

	const std::vector<const char *> m_DeviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	const std::string m_VertexShaderPath = "Shaders/Shader.vert.spv";
	const std::string m_FragmentShaderPath = "Shaders/Shader.frag.spv";

	VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
	
	VkInstance m_Instance = VK_NULL_HANDLE;
	/** This object will be implicitly destroyed when the VkInstance is destroyed */
	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

	VkDevice m_Device = VK_NULL_HANDLE;
	/** Device queues are implicitly destroyed when the device is destroyed */
	VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
	VkQueue m_PresentQueue = VK_NULL_HANDLE;

	VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
	/** The images were created by the implementation for the swap chain and 
	* they will be automatically cleaned up once the swap chain has been destroyed.
	*/
	std::vector<VkImage> m_SwapChainImages;
	std::vector<VkImageView> m_SwapChainImageViews;
	VkFormat m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_SwapChainExtent = { 0, 0 };

	VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> m_SwapChainFramebuffers;

	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	/** Command buffers will be automatically freed when their command pool is destroyed. */
	std::vector<VkCommandBuffer> m_CommandBuffers;

	const int m_MaxFramesInFlights = 2;
	std::vector<VkSemaphore> m_ImageAvailableSemaphores;
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;
	size_t m_CurrentFrame = 0;

protected:
	struct Vertex
	{
		glm::vec2 Position;
		glm::vec3 Color;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescription();
	};

	const std::vector<Vertex> m_Vertices = 
	{
		{ { -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },
		{ {  0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
		{ {  0.5f,  0.5f },{ 0.0f, 0.0f, 1.0f } },
		{ { -0.5f,  0.5f },{ 1.0f, 1.0f, 1.0f } }
	};

	const std::vector<uint32_t> m_Indices = 
	{
		0, 1, 2,
		2, 3, 0
	};

	VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;

protected:
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 Model;
		alignas(16) glm::mat4 View;
		alignas(16) glm::mat4 Projection;
	};

	VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkBuffer> m_UniformBuffers;
	std::vector<VkDeviceMemory> m_UniformBufferMemories;
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	/** Descriptor sets will be automatically freed when the descriptor pool is destroyed. */
	std::vector<VkDescriptorSet> m_DescriptorSets;

protected:
	const std::string m_TexturePath = "Textures/Texture.jpg";
	VkImage m_TextureImage = VK_NULL_HANDLE;
	VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
};