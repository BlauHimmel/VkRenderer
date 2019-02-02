#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>
#include <cstdint>

#include "Namespace.hpp"

NAMESPACE_BEGIN(GLOBAL_NAMESPACE)

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

bool CheckValidationLayerSupport(
	const std::vector<const char *> & Layers
);

std::vector<const char *> GetRequiredExtensions(
	bool bEnableValidationLayers
);

bool IsPhysicalDeviceSuitable(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface,
	const std::vector<const char *> Extensions
);

QueueFamilyIndices FindQueueFamilies(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
);

bool CheckPhysicalDeviceExtensionsSupport(
	VkPhysicalDevice Device,
	const std::vector<const char *> Extensions
);

SwapChainSupportDetails QuerySwapChainSupport(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
);

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
	const std::vector<VkSurfaceFormatKHR> & AvailableFormats
);

VkPresentModeKHR ChooseSwapPresentMode(
	const std::vector<VkPresentModeKHR> & AvailablePresentModes
);

VkExtent2D ChooseSwapExtent(
	GLFWwindow * pWindow,
	const VkSurfaceCapabilitiesKHR & Capabilities,
	uint32_t InitWidth,
	uint32_t InitHeight
);

void CreateImageView(
	VkDevice Device,
	VkImage Image,
	VkFormat Format,
	uint32_t MipLevels,
	VkImageAspectFlags AspectFlags,
	VkImageView & ImageView
);

VkShaderModule CreateShaderModule(
	VkDevice Device,
	const std::vector<char> & ShaderCode
);

VkFormat FindSupportedFormat(
	VkPhysicalDevice Device,
	const std::vector<VkFormat> & Candidates,
	VkImageTiling Tiling,
	VkFormatFeatureFlags Features
);

VkFormat FindDepthFormat(
	VkPhysicalDevice Device
);

bool HasStencilComponent(
	VkFormat Format
);

uint32_t FindMemoryType(
	VkPhysicalDevice Device,
	uint32_t TypeFilter,
	VkMemoryPropertyFlags Properties
);

void CreateBuffer(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	VkDeviceSize Size,
	VkBufferUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	VkBuffer & Buffer,
	VkDeviceMemory & BufferMemory
);

void CopyBuffer(
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	VkBuffer SrcBuffer,
	VkBuffer DstBuffer,
	VkDeviceSize Size
);

void CreateImage(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	uint32_t Width,
	uint32_t Height,
	uint32_t MipLevels,
	VkSampleCountFlagBits Samples,
	VkFormat Format,
	VkImageTiling Tiling,
	VkImageUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	VkImage & Image,
	VkDeviceMemory & ImageMemory
);

VkCommandBuffer BeginSingleTimeCommands(
	VkDevice Device,
	VkCommandPool CommandPool
);

void EndSingleTimeCommands(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkCommandBuffer CommandBuffer
);

void TransitionImageLayout(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkImage Image,
	VkFormat Format,
	uint32_t MipLevels,
	VkImageLayout OldLayout,
	VkImageLayout NewLayout
);

void CopyBufferToImage(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkBuffer SrcBuffer,
	VkImage DstImage,
	uint32_t Width,
	uint32_t Height
);

void GenerateMipmaps(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	VkImage Image,
	VkFormat Format,
	uint32_t Width,
	uint32_t Height,
	uint32_t MipLevels
);

VkSampleCountFlagBits GetMaxUsableSampleCount(
	VkPhysicalDevice Device
);

namespace ProxyVulkanFunction
{

VkResult vkCreateDebugUtilsMessengerEXT(
	VkInstance Instance,
	const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
	const VkAllocationCallbacks * pAllocator,
	VkDebugUtilsMessengerEXT * pDebugMessenger
);

void vkDestroyDebugUtilsMessengerEXT(
	VkInstance Instance,
	VkDebugUtilsMessengerEXT DebugMessenger,
	const VkAllocationCallbacks * pAllocator
);

}

NAMESPACE_END