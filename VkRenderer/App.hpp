#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/**
* The perspective projection matrix generated by GLM will use the OpenGL
* depth range of -1.0 to 1.0 by default. We need to configure it to use
* the Vulkan range of 0.0 to 1.0 using the GLM_FORCE_DEPTH_ZERO_TO_ONE
* definition.
*/
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>

//TODO :
//    1. Update the vertex/index buffer while the app is running.
//    2. Update the texture while the app is running.
//    3. Add UI.
//    4. Add lighting.
//    5. PBR material.
//    6. Use environment map.
//    7. Implement IBR.

/** An very simple vulkan application. */
class App
{
public:
	void Run();

protected:
	/** App */void InitWindow();
	/** App */void InitVulkan();
	/** App */void MainLoop();
	/** App */void Draw();
	/** App */void Destroy();

protected:
	/** App Helper */void UpdateUniformBuffer(
		uint32_t CurrentImage
	);

	/** Recreate the swapchain and all the objects depend on it. */
	/** App Helper */void RecreateSwapChainAndRelevantObject();

	/** Destroy the objects that are to be recreated in the RecreateSwapChain(). */
	/** App Helper */void DestroySwapChainAndRelevantObject();

protected:
	/** Vulkan Init */void CreateInstance();
	
	/** Vulkan Init */void SetupDebugMessenger();

	/** Vulkan Init */void CreateSurface();

	/** Vulkan Init */void SelectPhysicalDevice();

	/** Vulkan Init */void CreateLogicalDevice();

	/** Vulkan Init */void CreateSwapChain();
	
	/** Vulkan Init */void CreateSwapChainImageViews();
	
	/** Vulkan Init */void CreateRenderPass();

	/** Vulkan Init */void CreateDescriptorSetLayout();

	/** Vulkan Init */void CreateGraphicsPipeline();
	
	/** Vulkan Init */void CreateCommandPool();

	/** Vulkan Init */void CreateDepthResource();

	/** Vulkan Init */void CreateFramebuffers();

	/** Vulkan Init */void CreateTextureImage();
	
	/** Vulkan Init */void CreateTextureImageView();

	/** Vulkan Init */void CreateTextureSampler();

	/** Vulkan Init */void LoadObjModel();
	 
	/** Vulkan Init */void CreateVertexBuffer();

	/** Vulkan Init */void CreateIndexBuffer();

	/** Vulkan Init */void CreateUniformBuffer();

	/** Vulkan Init */void CreateDescriptorPool();

	/** Vulkan Init */void CreateDescriptorSets();

	/** Vulkan Init */void CreateCommandBuffers();

	/** Vulkan Init */void CreateSyncObjects();

protected:
	/** Helper Struct */struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;

		bool IsComplete() const;
	};

	/** Helper Struct */struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

protected:
	/** Helper */bool CheckValidationLayerSupport() const;

	/** Helper */std::vector<const char *> GetRequiredExtensions() const;

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

	/** Helper */void CreateImageView(
		VkDevice Device,
		VkImage Image,
		VkFormat Format,
		VkImageAspectFlags AspectFlags,
		VkImageView & ImageView
	);

	/** Helper */VkShaderModule CreateShaderModule(
		VkDevice Device,
		const std::vector<char> & ShaderCode
	);

	/** Helper */VkFormat FindSupportedFormat(
		VkPhysicalDevice Device,
		const std::vector<VkFormat> & Candidates,
		VkImageTiling Tiling,
		VkFormatFeatureFlags Features
	) const;

	/** Helper */VkFormat FindDepthFormat();

	/** Helper */bool HasStencilComponent(
		VkFormat Format
	) const;

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

protected: /** App */
	GLFWwindow * m_pWindow = nullptr;
	uint32_t m_InitWidth = 800;
	uint32_t m_InitHeight = 600;
	std::string m_Title = "Vulkan";
	std::string m_AppName = "VulkanApp";
	std::string m_EngineName = "VulkanEngine";
	std::string m_GpuName = "";
	bool m_bFramebufferResized = false;
	double m_FPS = 0.0f;

protected: /** Vulkan pipeline */
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

	VkImage m_DepthImage = VK_NULL_HANDLE;
	VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
	VkImageView m_DepthImageView = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> m_SwapChainFramebuffers;

	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	/** Command buffers will be automatically freed when their command pool is destroyed. */
	std::vector<VkCommandBuffer> m_CommandBuffers;

	const int m_MaxFramesInFlights = 2;
	std::vector<VkSemaphore> m_ImageAvailableSemaphores;
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;
	size_t m_CurrentFrame = 0;

protected: /** Mesh */
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Color;
		glm::vec3 Normal;
		glm::vec2 TexCoord;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescription();
	};

	struct VertexHash
	{
		size_t operator()(const Vertex & Rhs) const;
	};

	struct VertexEqual
	{
		bool operator()(const Vertex & Lhs, const Vertex & Rhs) const;
	};

	const std::string m_ModelPath = "Models/chalet.obj";
	std::vector<Vertex> m_Vertices;
	std::vector<uint32_t> m_Indices;

	size_t m_VertexNum = 0;
	size_t m_FacetNum = 0;

	VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;

protected: /** UBO */
	struct UniformBufferObject
	{
		glm::mat4 Model;
		glm::mat4 View;
		glm::mat4 Projection;
	};

	VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkBuffer> m_UniformBuffers;
	std::vector<VkDeviceMemory> m_UniformBufferMemories;
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	/** Descriptor sets will be automatically freed when the descriptor pool is destroyed. */
	std::vector<VkDescriptorSet> m_DescriptorSets;

protected: /** Texture */
	const std::string m_TexturePath = "Textures/chalet.jpg";
	VkImage m_TextureImage = VK_NULL_HANDLE;
	VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
	VkImageView m_TextureImageView = VK_NULL_HANDLE;
	VkSampler m_TextureSamler = VK_NULL_HANDLE;;
};