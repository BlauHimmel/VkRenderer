#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <memory>
#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <limits>
#include <cmath>
#include <algorithm>
#include <fstream>

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

protected:
	/** Step 1 */
	void CreateInstance();
	/** Helper */bool CheckValidationLayerSupport() const;
	/** Helper */std::vector<const char *> GetRequiredExtensions() const;

	/** Step 2 */
	void SetupDebugMessenger();

	/** Step 3 */
	void CreateSurface();

	/** Step 4 */
	void SelectPhysicalDevice();

	/** Step 5 */
	void CreateLogicalDevice();
	/** Helper */bool IsPhysicalDeviceSuitable(VkPhysicalDevice Device, VkSurfaceKHR Surface) const;
	/** Helper */QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device) const;
	/** Helper */bool CheckPhysicalDeviceExtensionsSupport(VkPhysicalDevice Device) const;
	/** Helper */SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device, VkSurfaceKHR Surface) const;

	/** Step 6 */
	void CreateSwapChain();
	/** Helper */VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> & AvailableFormats) const;
	/** Helper */VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> & AvailablePresentModes) const;
	/** Helper */VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR & Capabilities) const;
	
	/** Step 7 */
	void CreateSwapChainImageViews();

	/** Step 8 */
	void CreateRenderPass();

	/** Step 9 */
	void CreateGraphicsPipeline();
	/** Helper */VkShaderModule CreateShaderModule(VkDevice Device, const std::vector<char> & ShaderCode);

	/** Step 10 */
	void CreateFramebuffers();

	/** Step 11 */
	void CreateCommandPool();

	/** Step 12 */
	void CreateCommandBuffers();

	/** Step 13 */
	void CreateSyncObjects();

protected:
	/** Callback */static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT MessageType,
		const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
		void * pUserData
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

	/** Helper */static std::vector<char> ReadFile(const std::string & Filename);

protected:
	GLFWwindow * m_pWindow = nullptr;
	uint32_t m_Width = 800;
	uint32_t m_Height = 600;
	std::string m_Title = "Vulkan";
	std::string m_AppName = "VulkanApp";
	std::string m_EngineName = "VulkanEngine";

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
	std::vector<VkImage> m_SwapChainImages;
	std::vector<VkImageView> m_SwapChainImageViews;
	VkFormat m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_SwapChainExtent = { 0, 0 };

	VkRenderPass m_RenderPass;
	VkPipelineLayout m_PipelineLayout;
	VkPipeline m_GraphicsPipeline;

	std::vector<VkFramebuffer> m_SwapChainFramebuffers;

	VkCommandPool m_CommandPool;
	std::vector<VkCommandBuffer> m_CommandBuffers;

	const int m_MaxFramesInFlights = 2;
	std::vector<VkSemaphore> m_ImageAvailableSemaphores;
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;
	size_t m_CurrentFrame = 0;

};