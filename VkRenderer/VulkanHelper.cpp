#include "VulkanHelper.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <memory>
#include <set>
#include <string>
#include <algorithm>
#include <iostream>

NAMESPACE_BEGIN(GLOBAL_NAMESPACE)

bool QueueFamilyIndices::IsComplete() const
{
	return GraphicsFamily.has_value() && PresentFamily.has_value();
}

size_t SwapChainInfo::BufferCount() const
{
	return SwapChainImages.size();
}

VkDescriptorImageInfo TextureInfo::GetDescriptorImageInfo() const
{
	VkDescriptorImageInfo ImageInfo = {};
	ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ImageInfo.imageView = TextureImageView;
	ImageInfo.sampler = TextureSampler;
	return ImageInfo;
}

bool CheckValidationLayerSupport(
	const std::vector<const char *> & Layers
)
{
	uint32_t LayerCount = 0;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
	std::unique_ptr<VkLayerProperties[]> AvailableLayers(new VkLayerProperties[LayerCount]);
	vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.get());

	for (const auto & LayerName : Layers)
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

std::vector<const char *> GetRequiredExtensions(
	bool bEnableValidationLayers
)
{
	uint32_t GlfwExtensionCount = 0;
	const char ** ppGlfwExtensions = glfwGetRequiredInstanceExtensions(&GlfwExtensionCount);

	std::vector<const char *> Extensions(ppGlfwExtensions, ppGlfwExtensions + GlfwExtensionCount);

	if (bEnableValidationLayers)
	{
		Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return Extensions;
}

bool IsPhysicalDeviceSuitable(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface,
	const std::vector<const char *> Extensions
)
{
	QueueFamilyIndices Indices = FindQueueFamilies(Device, Surface);
	bool bExtensionsSupported = CheckPhysicalDeviceExtensionsSupport(Device, Extensions);
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

QueueFamilyIndices FindQueueFamilies(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
)
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
		vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, Surface, &bPresentSupport);
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

bool CheckPhysicalDeviceExtensionsSupport(
	VkPhysicalDevice Device,
	const std::vector<const char *> Extensions
)
{
	uint32_t ExtensionCount = 0;
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, nullptr);
	std::unique_ptr<VkExtensionProperties[]> AvailableExtensions(new VkExtensionProperties[ExtensionCount]);
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, AvailableExtensions.get());

	std::set<std::string> RequiredExtensions(Extensions.begin(), Extensions.end());
	for (uint32_t i = 0; i < ExtensionCount; i++)
	{
		RequiredExtensions.erase(AvailableExtensions[i].extensionName);
	}

	return RequiredExtensions.empty();
}

SwapChainSupportDetails QuerySwapChainSupport(
	VkPhysicalDevice Device,
	VkSurfaceKHR Surface
)
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

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
	const std::vector<VkSurfaceFormatKHR>& AvailableFormats
)
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

VkPresentModeKHR ChooseSwapPresentMode(
	const std::vector<VkPresentModeKHR>& AvailablePresentModes
)
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

VkExtent2D ChooseSwapExtent(
	GLFWwindow * pWindow,
	const VkSurfaceCapabilitiesKHR & Capabilities,
	uint32_t InitWidth,
	uint32_t InitHeight
)
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
			static_cast<uint32_t>(InitWidth),
			static_cast<uint32_t>(InitHeight)
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

void CreateImageView(
	VkDevice Device,
	VkImage Image,
	VkFormat Format,
	uint32_t MipLevels,
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
	CreateInfo.subresourceRange.levelCount = MipLevels;
	CreateInfo.subresourceRange.baseArrayLayer = 0;
	CreateInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(Device, &CreateInfo, nullptr, &ImageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture image view!");
	}
}

VkShaderModule CreateShaderModule(
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

VkFormat FindSupportedFormat(
	VkPhysicalDevice Device,
	const std::vector<VkFormat>& Candidates,
	VkImageTiling Tiling,
	VkFormatFeatureFlags Features
)
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

VkFormat FindDepthFormat(
	VkPhysicalDevice Device
)
{
	return FindSupportedFormat(
		Device,
		{
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool HasStencilComponent(
	VkFormat Format
)
{
	return Format == VK_FORMAT_D32_SFLOAT_S8_UINT || Format == VK_FORMAT_D24_UNORM_S8_UINT;
}

uint32_t FindMemoryType(
	VkPhysicalDevice Device,
	uint32_t TypeFilter,
	VkMemoryPropertyFlags Properties
)
{
	VkPhysicalDeviceMemoryProperties MemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(Device, &MemoryProperties);

	for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++)
	{
		if (TypeFilter & (1 << i) && (MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

void CreateBuffer(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	VkDeviceSize Size,
	VkBufferUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	BufferInfo & Buffer
)
{
	VkBufferCreateInfo BufferCreateInfo = {};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = Size;
	BufferCreateInfo.usage = Usage;
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(Device, &BufferCreateInfo, nullptr, &Buffer.Buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer!");
	}

	VkMemoryRequirements MemoryRequirements;
	vkGetBufferMemoryRequirements(Device, Buffer.Buffer, &MemoryRequirements);

	VkMemoryAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemoryRequirements.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(PhysicalDevice, MemoryRequirements.memoryTypeBits, Properties);

	if (vkAllocateMemory(Device, &AllocInfo, nullptr, &Buffer.Memory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory!");
	}

	vkBindBufferMemory(Device, Buffer.Buffer, Buffer.Memory, 0);
}

void CopyBuffer(
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	BufferInfo SrcBuffer,
	BufferInfo DstBuffer,
	VkDeviceSize Size
)
{
	VkCommandBuffer CommandBuffer = BeginSingleTimeCommands(Device, CommandPool);

	VkBufferCopy CopyRegion = {};
	CopyRegion.srcOffset = 0;
	CopyRegion.dstOffset = 0;
	CopyRegion.size = Size;
	vkCmdCopyBuffer(CommandBuffer, SrcBuffer.Buffer, DstBuffer.Buffer, 1, &CopyRegion);

	EndSingleTimeCommands(Device, Queue, CommandPool, CommandBuffer);
}

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
)
{
	VkImageCreateInfo ImageCreateInfo = {};
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageCreateInfo.extent.width = Width;
	ImageCreateInfo.extent.height = Height;
	ImageCreateInfo.extent.depth = 1;
	ImageCreateInfo.mipLevels = MipLevels;
	ImageCreateInfo.arrayLayers = 1;
	ImageCreateInfo.format = Format;
	ImageCreateInfo.tiling = Tiling;
	ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageCreateInfo.usage = Usage;
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.samples = Samples;

	if (vkCreateImage(Device, &ImageCreateInfo, nullptr, &Image) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture image!");
	}

	VkMemoryRequirements MemRequirements;
	vkGetImageMemoryRequirements(Device, Image, &MemRequirements);

	VkMemoryAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemRequirements.size;
	AllocInfo.memoryTypeIndex = FindMemoryType(PhysicalDevice, MemRequirements.memoryTypeBits, Properties);

	if (vkAllocateMemory(Device, &AllocInfo, nullptr, &ImageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture image memory!");
	}

	vkBindImageMemory(Device, Image, ImageMemory, 0);
}

VkCommandBuffer BeginSingleTimeCommands(
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

void EndSingleTimeCommands(
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

void TransitionImageLayout(
	VkDevice Device,
	VkQueue Queue,
	VkCommandPool CommandPool,
	VkImage Image,
	VkFormat Format,
	uint32_t MipLevels,
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
	Barrier.subresourceRange.levelCount = MipLevels;
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
	else if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		NewLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		Barrier.srcAccessMask = 0;
		Barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		DstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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

void CopyBufferToImage(
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
)
{
	/** Check if image format supports linear blitting */
	VkFormatProperties FormatProperties;
	vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, &FormatProperties);

	if (!(FormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Texture image format does not support linear blitting!");
	}

	VkCommandBuffer CommandBuffer = BeginSingleTimeCommands(Device, CommandPool);

	VkImageMemoryBarrier Barrier = {};
	Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	Barrier.image = Image;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Barrier.subresourceRange.baseArrayLayer = 0;
	Barrier.subresourceRange.layerCount = 1;
	Barrier.subresourceRange.levelCount = 1;

	int32_t MipWidth = Width;
	int32_t MipHeight = Height;

	for (uint32_t i = 1; i < MipLevels; i++)
	{
		Barrier.subresourceRange.baseMipLevel = i - 1;
		Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			CommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&Barrier
		);

		VkImageBlit Blit = {};
		Blit.srcOffsets[0] = { 0, 0, 0 };
		Blit.srcOffsets[1] = { MipWidth, MipHeight, 1 };
		Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Blit.srcSubresource.mipLevel = i - 1;
		Blit.srcSubresource.baseArrayLayer = 0;
		Blit.srcSubresource.layerCount = 1;
		Blit.dstOffsets[0] = { 0, 0, 0 };
		Blit.dstOffsets[1] =
		{
			MipWidth > 1 ? MipWidth / 2 : 1,
			MipHeight > 1 ? MipHeight / 2 : 1,
			1
		};
		Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Blit.dstSubresource.mipLevel = i;
		Blit.dstSubresource.baseArrayLayer = 0;
		Blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(
			CommandBuffer,
			Image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&Blit,
			VK_FILTER_LINEAR
		);

		Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			CommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&Barrier
		);

		if (MipWidth > 1)
		{
			MipWidth /= 2;
		}

		if (MipHeight > 1)
		{
			MipHeight /= 2;
		}
	}

	Barrier.subresourceRange.baseMipLevel = MipLevels - 1;
	Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		CommandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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

VkSampleCountFlagBits GetMaxUsableSampleCount(
	VkPhysicalDevice Device
)
{
	VkPhysicalDeviceProperties PhysicalDeviceProperties;
	vkGetPhysicalDeviceProperties(Device, &PhysicalDeviceProperties);

	VkSampleCountFlags Counts = std::min(
		PhysicalDeviceProperties.limits.framebufferColorSampleCounts,
		PhysicalDeviceProperties.limits.framebufferDepthSampleCounts
	);

	if (Counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	else if (Counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	else if (Counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	else if (Counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	else if (Counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	else if (Counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
	else { return VK_SAMPLE_COUNT_1_BIT; }
}

void CreateTextureImageFromFile(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	const char * pFilename,
	uint32_t & MipLevels,
	VkImage & TextureImage,
	VkDeviceMemory & TextureImageMemory
)
{
	int TexWidth = -1, TexHeight = -1, TexChannels = -1;
	stbi_uc * pPixels = stbi_load(
		pFilename,
		&TexWidth,
		&TexHeight,
		&TexChannels,
		STBI_rgb_alpha
	);
	VkDeviceSize ImageSize = TexWidth * TexHeight * 4;
	MipLevels = static_cast<uint32_t>(
		std::floor(std::log2(std::max(TexWidth, TexHeight)))
		) + 1;

	if (pPixels == nullptr)
	{
		pPixels = (stbi_uc *)(malloc(sizeof(stbi_uc) * 4));
		pPixels[0] = 255; pPixels[1] = 255; pPixels[2] = 255; pPixels[3] = 255;
		TexWidth = 1;
		TexHeight = 1;
		TexChannels = 4;
		ImageSize = 4;
		MipLevels = 1;
	}

	BufferInfo StagingBuffer;

	CreateBuffer(
		PhysicalDevice,
		Device,
		ImageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		StagingBuffer
	);

	MapMemory(Device, StagingBuffer.Memory, ImageSize, pPixels);

	stbi_image_free(pPixels);

	CreateImage(
		PhysicalDevice,
		Device,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight),
		MipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		TextureImage,
		TextureImageMemory
	);

	TransitionImageLayout(
		Device,
		Queue,
		CommandPool,
		TextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		MipLevels,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	CopyBufferToImage(
		Device,
		Queue,
		CommandPool,
		StagingBuffer.Buffer,
		TextureImage,
		static_cast<uint32_t>(TexWidth),
		static_cast<uint32_t>(TexHeight)
	);

	GenerateMipmaps(
		PhysicalDevice,
		Device,
		CommandPool,
		Queue,
		TextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		TexWidth,
		TexHeight,
		MipLevels
	);

	DestroyBuffer(Device, StagingBuffer);
}

void CreateTextureFromFile(
	VkPhysicalDevice PhysicalDevice,
	VkDevice Device,
	VkCommandPool CommandPool,
	VkQueue Queue,
	const char * pFilename,
	TextureInfo & Texture
)
{
	CreateTextureImageFromFile(
		PhysicalDevice,
		Device,
		CommandPool,
		Queue,
		pFilename,
		Texture.MipLevels,
		Texture.TextureImage,
		Texture.TextureImageMemory
	);

	CreateImageView(
		Device,
		Texture.TextureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		Texture.MipLevels,
		VK_IMAGE_ASPECT_COLOR_BIT,
		Texture.TextureImageView
	);

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
	CreateInfo.maxLod = static_cast<float>(Texture.MipLevels);

	if (vkCreateSampler(Device, &CreateInfo, nullptr, &Texture.TextureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler!");
	}
}

void DestroyTexture(
	VkDevice Device,
	TextureInfo & Texture
)
{
	vkDestroySampler(Device, Texture.TextureSampler, nullptr);
	vkDestroyImageView(Device, Texture.TextureImageView, nullptr);
	vkDestroyImage(Device, Texture.TextureImage, nullptr);
	vkFreeMemory(Device, Texture.TextureImageMemory, nullptr);
}

void DestroyBuffer(
	VkDevice Device,
	BufferInfo & Buffer
)
{
	vkDestroyBuffer(Device, Buffer.Buffer, nullptr);
	vkFreeMemory(Device, Buffer.Memory, nullptr);
}

void MapMemory(
	VkDevice Device,
	VkDeviceMemory Memory,
	VkDeviceSize Size,
	void * pData
)
{
	void * pMappedData = nullptr;
	vkMapMemory(Device, Memory, 0, Size, 0, &pMappedData);
	memcpy(pMappedData, pData, Size);
	vkUnmapMemory(Device, Memory);
}

NAMESPACE_BEGIN(ProxyVulkanFunction)

VkResult vkCreateDebugUtilsMessengerEXT(
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
		std::cerr << "Function vkCreateDebugUtilsMessengerEXT not found!" << std::endl;
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void vkDestroyDebugUtilsMessengerEXT(
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
	else
	{
		std::cerr << "Function vkDestroyDebugUtilsMessengerEXT not found!" << std::endl;
	}
}

NAMESPACE_END

NAMESPACE_END
