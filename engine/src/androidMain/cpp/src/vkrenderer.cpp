#include "vkrenderer.h"
#include <android/native_window_jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <cstring>
#include <utility>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <cstddef>
#include <mutex>

#include "ImageLoader.h"

static constexpr size_t INVALID_TEXTURE_INDEX = std::numeric_limits<size_t>::max();
#include "ModelLoader.h"
#include "VulkanBuilder.h"
#include "Camera.h"

#define LOG_TAG "VKRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct GpuModel {
	Model cpu;
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexMemory = VK_NULL_HANDLE;
	std::vector<size_t> materialTextureIndices;
};

struct TextureResource {
	std::string key;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct VulkanState {
	ANativeWindow* window = nullptr;
	AAssetManager* assetManager = nullptr;
	VulkanBuilder* builder = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat{};
	VkExtent2D swapchainExtent{};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;
	bool initialized = false;
	Camera camera;
	std::vector<GpuModel> models;
	VkDescriptorSetLayout textureDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
	VkCommandPool uploadCommandPool = VK_NULL_HANDLE;
	std::vector<TextureResource> textures;
	std::unordered_map<std::string, size_t> textureCache;
	size_t defaultTextureIndex = INVALID_TEXTURE_INDEX;
};

static VulkanState g;
static std::mutex g_stateMutex;

JavaVM* g_javaVm = nullptr;
jclass g_engineApiClass = nullptr;
jmethodID g_decodeTextureMethod = nullptr;

static void check(VkResult r, const char* what) {
	if (r != VK_SUCCESS) {
		LOGE("Vulkan error %d at %s", r, what);
		abort();
	}
}

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(g.physicalDevice, &memProps);
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	LOGE("Failed to find suitable memory type");
	abort();
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
	VkBufferCreateInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	check(vkCreateBuffer(g.device, &bi, nullptr, &buffer), "vkCreateBuffer");

	VkMemoryRequirements req{};
	vkGetBufferMemoryRequirements(g.device, buffer, &req);

	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
	check(vkAllocateMemory(g.device, &ai, nullptr, &memory), "vkAllocateMemory");
	check(vkBindBufferMemory(g.device, buffer, memory, 0), "vkBindBufferMemory");
}

static void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& memory) {
	VkImageCreateInfo ici{};
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.format = format;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ici.usage = usage;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	check(vkCreateImage(g.device, &ici, nullptr, &image), "vkCreateImage");

	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(g.device, image, &memReq);

	VkMemoryAllocateInfo alloc{};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	check(vkAllocateMemory(g.device, &alloc, nullptr, &memory), "vkAllocateMemory(image)");
	check(vkBindImageMemory(g.device, image, memory, 0), "vkBindImageMemory");
}

static VkImageView createImageView(VkImage image, VkFormat format) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	VkImageView imageView = VK_NULL_HANDLE;
	check(vkCreateImageView(g.device, &viewInfo, nullptr, &imageView), "vkCreateImageView(texture)");
	return imageView;
}

static VkSampler createSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	VkSampler sampler = VK_NULL_HANDLE;
	check(vkCreateSampler(g.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
	return sampler;
}

static VkCommandBuffer beginSingleTimeCommands() {
	if (!g.device || !g.uploadCommandPool) return VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo alloc{};
	alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc.commandPool = g.uploadCommandPool;
	alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	check(vkAllocateCommandBuffers(g.device, &alloc, &cmd), "vkAllocateCommandBuffers(single)");

	VkCommandBufferBeginInfo begin{};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	check(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer(single)");
	return cmd;
}

static void endSingleTimeCommands(VkCommandBuffer cmd) {
	if (!cmd) return;
	check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer(single)");
	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	VkResult submitResult = vkQueueSubmit(g.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
	if (submitResult == VK_ERROR_DEVICE_LOST) {
		LOGE("vkQueueSubmit(single) reported VK_ERROR_DEVICE_LOST");
		vkFreeCommandBuffers(g.device, g.uploadCommandPool, 1, &cmd);
		return;
	}
	check(submitResult, "vkQueueSubmit(single)");
	check(vkQueueWaitIdle(g.graphicsQueue), "vkQueueWaitIdle(single)");
	vkFreeCommandBuffers(g.device, g.uploadCommandPool, 1, &cmd);
}

static void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	(void)format;
	VkCommandBuffer cmd = beginSingleTimeCommands();
	if (!cmd) return;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		LOGE("Unsupported image layout transition");
	}

	vkCmdPipelineBarrier(
		cmd,
		sourceStage,
		destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	endSingleTimeCommands(cmd);
}

static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer cmd = beginSingleTimeCommands();
	if (!cmd) return;

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(
		cmd,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	endSingleTimeCommands(cmd);
}

static void createUploadCommandPoolIfNeeded() {
	if (g.uploadCommandPool || !g.device) return;
	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = g.graphicsQueueFamily;
	info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	check(vkCreateCommandPool(g.device, &info, nullptr, &g.uploadCommandPool), "vkCreateCommandPool(upload)");
}

static void createTextureDescriptorSetLayoutIfNeeded() {
	if (g.textureDescriptorSetLayout || !g.device) return;
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 1;
	info.pBindings = &binding;
	check(vkCreateDescriptorSetLayout(g.device, &info, nullptr, &g.textureDescriptorSetLayout), "vkCreateDescriptorSetLayout(texture)");
}

static void createTextureDescriptorPoolIfNeeded() {
	if (g.textureDescriptorPool || !g.device) return;
	const uint32_t maxTextures = 256;
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = maxTextures;

	VkDescriptorPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = 1;
	info.pPoolSizes = &poolSize;
	info.maxSets = maxTextures;
	info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	check(vkCreateDescriptorPool(g.device, &info, nullptr, &g.textureDescriptorPool), "vkCreateDescriptorPool(texture)");
}

static size_t createTextureFromPixels(const std::string& key, uint32_t width, uint32_t height, const unsigned char* pixels, size_t size) {
	createUploadCommandPoolIfNeeded();
	createTextureDescriptorSetLayoutIfNeeded();
	createTextureDescriptorPoolIfNeeded();
	if (!g.device) return INVALID_TEXTURE_INDEX;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	createBuffer(static_cast<VkDeviceSize>(size), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingMemory);

	void* data = nullptr;
	check(vkMapMemory(g.device, stagingMemory, 0, static_cast<VkDeviceSize>(size), 0, &data), "vkMapMemory(texture staging)");
	std::memcpy(data, pixels, size);
	vkUnmapMemory(g.device, stagingMemory);

	TextureResource texture;
	texture.key = key;
	texture.width = width;
	texture.height = height;

	createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		texture.image, texture.memory);

	transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, texture.image, width, height);
	transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(g.device, stagingBuffer, nullptr);
	vkFreeMemory(g.device, stagingMemory, nullptr);

	texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM);
	texture.sampler = createSampler();

	VkDescriptorSetAllocateInfo alloc{};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = g.textureDescriptorPool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &g.textureDescriptorSetLayout;
	check(vkAllocateDescriptorSets(g.device, &alloc, &texture.descriptorSet), "vkAllocateDescriptorSets(texture)");

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture.view;
	imageInfo.sampler = texture.sampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = texture.descriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;
	vkUpdateDescriptorSets(g.device, 1, &write, 0, nullptr);

	const size_t index = g.textures.size();
	g.textures.push_back(texture);
	g.textureCache[key] = index;
	return index;
}

static size_t ensureTextureForMaterial(const Material& material) {
	std::string key;
	if (!material.diffuseTexture.empty()) {
		key = "file:" + material.diffuseTexture;
		auto it = g.textureCache.find(key);
		if (it != g.textureCache.end()) {
			return it->second;
		}

		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		if (LoadImageFromAssets(g.assetManager, material.diffuseTexture, 4, pixels, width, height)) {
			const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
			if (size == pixels.size()) {
				return createTextureFromPixels(key, static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels.data(), pixels.size());
			}
		} else {
			LOGE("Falling back to diffuse color for material %s", material.name.c_str());
		}
	}

	const float r = std::clamp(material.diffuseColor[0], 0.0f, 1.0f);
	const float gCol = std::clamp(material.diffuseColor[1], 0.0f, 1.0f);
	const float b = std::clamp(material.diffuseColor[2], 0.0f, 1.0f);
	const unsigned char r8 = static_cast<unsigned char>(r * 255.0f);
	const unsigned char g8 = static_cast<unsigned char>(gCol * 255.0f);
	const unsigned char b8 = static_cast<unsigned char>(b * 255.0f);

	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "color:%u_%u_%u", r8, g8, b8);
	key = buffer;
	auto itColor = g.textureCache.find(key);
	if (itColor != g.textureCache.end()) {
		return itColor->second;
	}

	unsigned char solid[4] = { r8, g8, b8, 255 };
	return createTextureFromPixels(key, 1, 1, solid, sizeof(solid));
}

static void destroyTextureResources() {
	if (!g.device) {
		g.textures.clear();
		g.textureCache.clear();
		g.defaultTextureIndex = INVALID_TEXTURE_INDEX;
		return;
	}
	for (auto& texture : g.textures) {
		if (texture.sampler) vkDestroySampler(g.device, texture.sampler, nullptr);
		if (texture.view) vkDestroyImageView(g.device, texture.view, nullptr);
		if (texture.image) vkDestroyImage(g.device, texture.image, nullptr);
		if (texture.memory) vkFreeMemory(g.device, texture.memory, nullptr);
	}
	g.textures.clear();
	g.textureCache.clear();
	if (g.textureDescriptorPool) {
		vkDestroyDescriptorPool(g.device, g.textureDescriptorPool, nullptr);
		g.textureDescriptorPool = VK_NULL_HANDLE;
	}
	if (g.textureDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(g.device, g.textureDescriptorSetLayout, nullptr);
		g.textureDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (g.uploadCommandPool) {
		vkDestroyCommandPool(g.device, g.uploadCommandPool, nullptr);
		g.uploadCommandPool = VK_NULL_HANDLE;
	}
	g.defaultTextureIndex = INVALID_TEXTURE_INDEX;
}

static size_t getDefaultTextureIndex() {
	if (g.defaultTextureIndex != INVALID_TEXTURE_INDEX) return g.defaultTextureIndex;
	Material material;
	material.name = "Default";
	material.diffuseColor = { 1.0f, 1.0f, 1.0f };
	material.diffuseTexture.clear();
	size_t index = ensureTextureForMaterial(material);
	if (index == INVALID_TEXTURE_INDEX) {
		return INVALID_TEXTURE_INDEX;
	}
	g.defaultTextureIndex = index;
	return index;
}

static void destroyGpuBuffers(GpuModel& gpuModel) {
	if (g.device && gpuModel.indexBuffer) {
		vkDestroyBuffer(g.device, gpuModel.indexBuffer, nullptr);
	}
	if (g.device && gpuModel.indexMemory) {
		vkFreeMemory(g.device, gpuModel.indexMemory, nullptr);
	}
	if (g.device && gpuModel.vertexBuffer) {
		vkDestroyBuffer(g.device, gpuModel.vertexBuffer, nullptr);
	}
	if (g.device && gpuModel.vertexMemory) {
		vkFreeMemory(g.device, gpuModel.vertexMemory, nullptr);
	}
	gpuModel.indexBuffer = VK_NULL_HANDLE;
	gpuModel.indexMemory = VK_NULL_HANDLE;
	gpuModel.vertexBuffer = VK_NULL_HANDLE;
	gpuModel.vertexMemory = VK_NULL_HANDLE;
}

static void destroyAllModelBuffers() {
	for (auto& model : g.models) {
		destroyGpuBuffers(model);
	}
}

static void uploadGpuBuffers(GpuModel& gpuModel) {
	if (!g.device) return;
	if (!gpuModel.cpu.hasGeometry()) return;

	destroyGpuBuffers(gpuModel);

	const size_t vertexCount = gpuModel.cpu.vertexCount();
	std::vector<float> interleaved;
	interleaved.reserve(vertexCount * 8);
	for (size_t i = 0; i < vertexCount; ++i) {
		const size_t posIndex = i * 3;
		const size_t normIndex = i * 3;
		const size_t uvIndex = i * 2;
		float px = gpuModel.cpu.positions.size() > posIndex ? gpuModel.cpu.positions[posIndex] : 0.0f;
		float py = gpuModel.cpu.positions.size() > posIndex + 1 ? gpuModel.cpu.positions[posIndex + 1] : 0.0f;
		float pz = gpuModel.cpu.positions.size() > posIndex + 2 ? gpuModel.cpu.positions[posIndex + 2] : 0.0f;
		float nx = gpuModel.cpu.normals.size() > normIndex ? gpuModel.cpu.normals[normIndex] : 0.0f;
		float ny = gpuModel.cpu.normals.size() > normIndex + 1 ? gpuModel.cpu.normals[normIndex + 1] : 0.0f;
		float nz = gpuModel.cpu.normals.size() > normIndex + 2 ? gpuModel.cpu.normals[normIndex + 2] : 0.0f;
		float u = gpuModel.cpu.texcoords.size() > uvIndex ? gpuModel.cpu.texcoords[uvIndex] : 0.0f;
		float v = gpuModel.cpu.texcoords.size() > uvIndex + 1 ? gpuModel.cpu.texcoords[uvIndex + 1] : 0.0f;
		interleaved.push_back(px);
		interleaved.push_back(py);
		interleaved.push_back(pz);
		interleaved.push_back(nx);
		interleaved.push_back(ny);
		interleaved.push_back(nz);
		interleaved.push_back(u);
		interleaved.push_back(1.0f - v); // Flip V for Vulkan coordinate system
	}

	VkDeviceSize vsize = sizeof(float) * interleaved.size();
	VkDeviceSize isize = sizeof(uint32_t) * gpuModel.cpu.indices.size();

	createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gpuModel.vertexBuffer, gpuModel.vertexMemory);
	createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gpuModel.indexBuffer, gpuModel.indexMemory);

	void* data = nullptr;
	check(vkMapMemory(g.device, gpuModel.vertexMemory, 0, vsize, 0, &data), "vkMapMemory(vertex)");
	std::memcpy(data, interleaved.data(), static_cast<size_t>(vsize));
	vkUnmapMemory(g.device, gpuModel.vertexMemory);

	check(vkMapMemory(g.device, gpuModel.indexMemory, 0, isize, 0, &data), "vkMapMemory(index)");
	std::memcpy(data, gpuModel.cpu.indices.data(), static_cast<size_t>(isize));
	vkUnmapMemory(g.device, gpuModel.indexMemory);

	gpuModel.materialTextureIndices.clear();
	gpuModel.materialTextureIndices.resize(gpuModel.cpu.materials.size(), INVALID_TEXTURE_INDEX);
	for (size_t i = 0; i < gpuModel.cpu.materials.size(); ++i) {
		size_t textureIndex = ensureTextureForMaterial(gpuModel.cpu.materials[i]);
		if (textureIndex == INVALID_TEXTURE_INDEX) {
			textureIndex = getDefaultTextureIndex();
		}
		gpuModel.materialTextureIndices[i] = textureIndex;
	}
}

static std::vector<uint32_t> loadSpirvFromAsset(const char* path) {
	std::vector<uint32_t> words;
	if (!g.assetManager) return words;
	AAsset* asset = AAssetManager_open(g.assetManager, path, AASSET_MODE_STREAMING);
	if (!asset) return words;
	off_t len = AAsset_getLength(asset);
	if (len > 0 && (len % 4 == 0)) {
		words.resize(static_cast<size_t>(len) / 4);
		int64_t read = AAsset_read(asset, words.data(), len);
		if (read != len) {
			words.clear();
		}
	}
	AAsset_close(asset);
	return words;
}

static void buildPipelineWithBuilder() {
	createTextureDescriptorSetLayoutIfNeeded();
	auto vert = loadSpirvFromAsset("shaders/triangle.vert.spv");
	auto frag = loadSpirvFromAsset("shaders/triangle.frag.spv");
	std::vector<VkDescriptorSetLayout> layouts;
	if (g.textureDescriptorSetLayout) {
		layouts.push_back(g.textureDescriptorSetLayout);
	}
	g.builder->setVertexSpirv(vert)
		.setFragmentSpirv(frag)
		.setDescriptorSetLayouts(layouts)
		.buildRenderPass()
		.buildPipeline();
	g.renderPass = g.builder->getRenderPass();
	g.pipelineLayout = g.builder->getPipelineLayout();
	g.graphicsPipeline = g.builder->getGraphicsPipeline();
}

static void createFramebuffers() {
	g.framebuffers.resize(g.swapchainImageViews.size());
	for (size_t i = 0; i < g.swapchainImageViews.size(); ++i) {
		VkImageView attachments[] = { g.swapchainImageViews[i] };
		VkFramebufferCreateInfo fci{};
		fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fci.renderPass = g.renderPass;
		fci.attachmentCount = 1;
		fci.pAttachments = attachments;
		fci.width = g.swapchainExtent.width;
		fci.height = g.swapchainExtent.height;
		fci.layers = 1;
		check(vkCreateFramebuffer(g.device, &fci, nullptr, &g.framebuffers[i]), "vkCreateFramebuffer");
	}
}

static void createCommandPoolBuffers() {
	VkCommandPoolCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.queueFamilyIndex = g.graphicsQueueFamily;
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	check(vkCreateCommandPool(g.device, &cpci, nullptr, &g.commandPool), "vkCreateCommandPool");

	g.commandBuffers.resize(g.framebuffers.size());
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = g.commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = static_cast<uint32_t>(g.commandBuffers.size());
	check(vkAllocateCommandBuffers(g.device, &ai, g.commandBuffers.data()), "vkAllocateCommandBuffers");
}

static void recordCommandBuffers() {
	for (size_t i = 0; i < g.commandBuffers.size(); ++i) {
		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		check(vkBeginCommandBuffer(g.commandBuffers[i], &bi), "vkBeginCommandBuffer");

		VkClearValue clear{};
		clear.color = { {0.1f, 0.2f, 0.3f, 1.0f} };
		VkRenderPassBeginInfo rpbi{};
		rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpbi.renderPass = g.renderPass;
		rpbi.framebuffer = g.framebuffers[i];
		rpbi.renderArea.offset = {0,0};
		rpbi.renderArea.extent = g.swapchainExtent;
		rpbi.clearValueCount = 1;
		rpbi.pClearValues = &clear;
		vkCmdBeginRenderPass(g.commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		// Apply camera viewport/scissor and draw
		g.camera.applyToCommandBuffer(g.commandBuffers[i]);
		vkCmdBindPipeline(g.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g.graphicsPipeline);
		for (const auto& model : g.models) {
			if (!model.cpu.hasGeometry() || !model.vertexBuffer || !model.indexBuffer) {
				continue;
			}

			float pushConstants[9] = {
				g.camera.getYaw(),
				g.camera.getPitch(),
				g.camera.getRoll(),
				g.camera.getWidth(),
				g.camera.getHeight(),
				g.camera.getDistance(),
				model.cpu.position[0],
				model.cpu.position[1],
				model.cpu.position[2]
			};
			vkCmdPushConstants(g.commandBuffers[i], g.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), pushConstants);

			VkDeviceSize offsets[] = {0};
			VkBuffer vertexBuffer = model.vertexBuffer;
			vkCmdBindVertexBuffers(g.commandBuffers[i], 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(g.commandBuffers[i], model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		const auto& subsets = model.cpu.subsets;
		if (!subsets.empty()) {
			for (const auto& subset : subsets) {
				size_t textureIndex = getDefaultTextureIndex();
				if (subset.materialIndex < model.materialTextureIndices.size()) {
					const size_t mapped = model.materialTextureIndices[subset.materialIndex];
					if (mapped != INVALID_TEXTURE_INDEX) {
						textureIndex = mapped;
					}
				}
				if (textureIndex >= g.textures.size()) {
					textureIndex = getDefaultTextureIndex();
				}
				if (textureIndex >= g.textures.size()) {
					continue;
				}
				VkDescriptorSet descriptorSet = g.textures[textureIndex].descriptorSet;
				if (!descriptorSet) continue;
				vkCmdBindDescriptorSets(g.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(g.commandBuffers[i], subset.indexCount, 1, subset.indexOffset, 0, 0);
			}
		} else {
			size_t textureIndex = getDefaultTextureIndex();
			if (!model.materialTextureIndices.empty()) {
				const size_t mapped = model.materialTextureIndices[0];
				if (mapped != INVALID_TEXTURE_INDEX) {
					textureIndex = mapped;
				}
			}
			if (textureIndex < g.textures.size()) {
				VkDescriptorSet descriptorSet = g.textures[textureIndex].descriptorSet;
				if (descriptorSet) {
					vkCmdBindDescriptorSets(g.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
					vkCmdDrawIndexed(g.commandBuffers[i], static_cast<uint32_t>(model.cpu.indexCount()), 1, 0, 0, 0);
				}
			}
		}
		}
		vkCmdEndRenderPass(g.commandBuffers[i]);
		check(vkEndCommandBuffer(g.commandBuffers[i]), "vkEndCommandBuffer");
	}
}

static void createSyncObjects() {
	size_t frames = g.commandBuffers.size();
	g.imageAvailable.resize(frames);
	g.renderFinished.resize(frames);
	g.inFlightFences.resize(frames);
	VkSemaphoreCreateInfo sci{ };
	sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fci{ };
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (size_t i = 0; i < frames; ++i) {
		check(vkCreateSemaphore(g.device, &sci, nullptr, &g.imageAvailable[i]), "vkCreateSemaphore(image)");
		check(vkCreateSemaphore(g.device, &sci, nullptr, &g.renderFinished[i]), "vkCreateSemaphore(render)");
		check(vkCreateFence(g.device, &fci, nullptr, &g.inFlightFences[i]), "vkCreateFence");
	}
}

static void cleanupSwapchain() {
	destroyAllModelBuffers();
	g.builder->cleanupSwapchain();
	for (auto f : g.framebuffers) if (f) vkDestroyFramebuffer(g.device, f, nullptr);
	g.framebuffers.clear();
}

static void recreateSwapchain(uint32_t width, uint32_t height) {
	std::lock_guard<std::mutex> guard(g_stateMutex);
	vkDeviceWaitIdle(g.device);
	cleanupSwapchain();
	g.builder->buildSwapchain(width, height)
		.buildImageViews();
	g.swapchain = g.builder->getSwapchain();
	g.swapchainFormat = g.builder->getSwapchainFormat();
	g.swapchainExtent = g.builder->getSwapchainExtent();
	g.swapchainImages = g.builder->getSwapchainImages();
	g.swapchainImageViews = g.builder->getSwapchainImageViews();
	g.camera.updateViewport(g.swapchainExtent);
	buildPipelineWithBuilder();
	for (auto& model : g.models) {
		uploadGpuBuffers(model);
	}
	createFramebuffers();
	createCommandPoolBuffers();
	recordCommandBuffers();
	createSyncObjects();
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject assetManager) {
	if (!g_javaVm) {
		env->GetJavaVM(&g_javaVm);
	}
	if (!g_engineApiClass) {
		jclass localClass = env->GetObjectClass(thiz);
		if (localClass) {
			g_engineApiClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
			env->DeleteLocalRef(localClass);
		}
	}
	if (g_engineApiClass && !g_decodeTextureMethod) {
		g_decodeTextureMethod = env->GetStaticMethodID(g_engineApiClass, "decodeTexture", "(Ljava/lang/String;)[B");
		if (!g_decodeTextureMethod) {
			LOGE("Failed to locate EngineAPI.decodeTexture");
		}
	}

	g.window = ANativeWindow_fromSurface(env, surface);
	g.assetManager = AAssetManager_fromJava(env, assetManager);
	uint32_t width = static_cast<uint32_t>(ANativeWindow_getWidth(g.window));
	uint32_t height = static_cast<uint32_t>(ANativeWindow_getHeight(g.window));

	// Use VulkanBuilder to set up Vulkan
	g.builder = new VulkanBuilder(g.window, g.assetManager);
	g.builder->buildInstance()
		.buildSurface()
		.pickPhysicalDevice()
		.buildDeviceAndQueue()
		.buildSwapchain(width, height)
		.buildImageViews();
	// Copy core handles for local use
	g.instance = g.builder->getInstance();
	g.surface = g.builder->getSurface();
	g.physicalDevice = g.builder->getPhysicalDevice();
	g.graphicsQueueFamily = g.builder->getGraphicsQueueFamily();
	g.device = g.builder->getDevice();
	g.graphicsQueue = g.builder->getGraphicsQueue();
	g.swapchain = g.builder->getSwapchain();
	g.swapchainFormat = g.builder->getSwapchainFormat();
	g.swapchainExtent = g.builder->getSwapchainExtent();
	g.swapchainImages = g.builder->getSwapchainImages();
	g.swapchainImageViews = g.builder->getSwapchainImageViews();
	g.camera.updateViewport(g.swapchainExtent);
	// Build render pass and pipeline via builder
	buildPipelineWithBuilder();
	// proceed with buffers/command buffers
	for (auto& model : g.models) {
		uploadGpuBuffers(model);
	}
	createFramebuffers();
	createCommandPoolBuffers();
	recordCommandBuffers();
	createSyncObjects();
	g.initialized = true;
	LOGI("Vulkan initialized w=%u h=%u", width, height);
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height) {
	if (!g.initialized) return;
	if (width <= 0 || height <= 0) return;
	recreateSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeLoadModel(JNIEnv* env, jobject thiz, jstring modelName, jfloat x, jfloat y, jfloat z) {
	std::lock_guard<std::mutex> guard(g_stateMutex);
	const char* modelChars = modelName ? env->GetStringUTFChars(modelName, nullptr) : nullptr;
	std::string modelPath = modelChars ? std::string(modelChars) : std::string();
	if (modelChars) {
		env->ReleaseStringUTFChars(modelName, modelChars);
	}

	if (modelPath.empty()) {
		LOGE("nativeLoadModel called with empty model path");
		return;
	}

	Model newModel = loadModel(g.assetManager, modelPath);
	newModel.setPosition(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

	if (!newModel.hasGeometry()) {
		LOGE("Model %s has no geometry and will be skipped", modelPath.c_str());
		return;
	}

	GpuModel gpuModel;
	gpuModel.cpu = std::move(newModel);

	if (g.initialized) {
		vkDeviceWaitIdle(g.device);
		uploadGpuBuffers(gpuModel);
	}

	g.models.push_back(std::move(gpuModel));
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeMoveCamera(JNIEnv* env, jobject thiz, jfloat delta) {
	if (!g.initialized) return;
	g.camera.move(static_cast<float>(delta));
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRender(JNIEnv* env, jobject thiz) {
	if (!g.initialized) return;
	std::unique_lock<std::mutex> stateLock(g_stateMutex);
	size_t i = g.currentFrame % g.commandBuffers.size();
	check(vkWaitForFences(g.device, 1, &g.inFlightFences[i], VK_TRUE, UINT64_MAX), "vkWaitForFences");
	check(vkResetFences(g.device, 1, &g.inFlightFences[i]), "vkResetFences");

	uint32_t imageIndex;
	VkResult acq = vkAcquireNextImageKHR(g.device, g.swapchain, UINT64_MAX, g.imageAvailable[i], VK_NULL_HANDLE, &imageIndex);
	if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
		return;
	}
	check(acq, "vkAcquireNextImageKHR");

	// Reset and record command buffer with current camera rotation
	check(vkResetCommandBuffer(g.commandBuffers[imageIndex], 0), "vkResetCommandBuffer");
	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	check(vkBeginCommandBuffer(g.commandBuffers[imageIndex], &bi), "vkBeginCommandBuffer");

	VkClearValue clear{};
	clear.color = { {0.1f, 0.2f, 0.3f, 1.0f} };
	VkRenderPassBeginInfo rpbi{};
	rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpbi.renderPass = g.renderPass;
	rpbi.framebuffer = g.framebuffers[imageIndex];
	rpbi.renderArea.offset = {0,0};
	rpbi.renderArea.extent = g.swapchainExtent;
	rpbi.clearValueCount = 1;
	rpbi.pClearValues = &clear;
	vkCmdBeginRenderPass(g.commandBuffers[imageIndex], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	
	// Apply camera viewport/scissor
	g.camera.applyToCommandBuffer(g.commandBuffers[imageIndex]);
	vkCmdBindPipeline(g.commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g.graphicsPipeline);
	for (const auto& model : g.models) {
		if (!model.cpu.hasGeometry() || !model.vertexBuffer || !model.indexBuffer) {
			continue;
		}

		float pushConstants[9] = {
			g.camera.getYaw(),
			g.camera.getPitch(),
			g.camera.getRoll(),
			g.camera.getWidth(),
			g.camera.getHeight(),
			g.camera.getDistance(),
			model.cpu.position[0],
			model.cpu.position[1],
			model.cpu.position[2]
		};
		vkCmdPushConstants(g.commandBuffers[imageIndex], g.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), pushConstants);

		VkDeviceSize offsets[] = {0};
		VkBuffer vertexBuffer = model.vertexBuffer;
		vkCmdBindVertexBuffers(g.commandBuffers[imageIndex], 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(g.commandBuffers[imageIndex], model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		const auto& subsets = model.cpu.subsets;
		if (!subsets.empty()) {
			for (const auto& subset : subsets) {
				size_t textureIndex = getDefaultTextureIndex();
				if (subset.materialIndex < model.materialTextureIndices.size()) {
					const size_t mapped = model.materialTextureIndices[subset.materialIndex];
					if (mapped != INVALID_TEXTURE_INDEX) {
						textureIndex = mapped;
					}
				}
				if (textureIndex >= g.textures.size()) {
					textureIndex = getDefaultTextureIndex();
				}
				if (textureIndex >= g.textures.size()) {
					continue;
				}
				VkDescriptorSet descriptorSet = g.textures[textureIndex].descriptorSet;
				if (!descriptorSet) continue;
				vkCmdBindDescriptorSets(g.commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(g.commandBuffers[imageIndex], subset.indexCount, 1, subset.indexOffset, 0, 0);
			}
		} else {
			size_t textureIndex = getDefaultTextureIndex();
			if (!model.materialTextureIndices.empty()) {
				const size_t mapped = model.materialTextureIndices[0];
				if (mapped != INVALID_TEXTURE_INDEX) {
					textureIndex = mapped;
				}
			}
			if (textureIndex < g.textures.size()) {
				VkDescriptorSet descriptorSet = g.textures[textureIndex].descriptorSet;
				if (descriptorSet) {
					vkCmdBindDescriptorSets(g.commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
					vkCmdDrawIndexed(g.commandBuffers[imageIndex], static_cast<uint32_t>(model.cpu.indexCount()), 1, 0, 0, 0);
				}
			}
		}
	}
	vkCmdEndRenderPass(g.commandBuffers[imageIndex]);
	check(vkEndCommandBuffer(g.commandBuffers[imageIndex]), "vkEndCommandBuffer");
	stateLock.unlock();

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &g.imageAvailable[i];
	submit.pWaitDstStageMask = &waitStage;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &g.commandBuffers[imageIndex];
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &g.renderFinished[i];
	VkResult submitResult = vkQueueSubmit(g.graphicsQueue, 1, &submit, g.inFlightFences[i]);
	if (submitResult == VK_ERROR_DEVICE_LOST) {
		LOGE("vkQueueSubmit reported VK_ERROR_DEVICE_LOST");
		return;
	}
	check(submitResult, "vkQueueSubmit");

	VkPresentInfoKHR present{};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &g.renderFinished[i];
	present.swapchainCount = 1;
	present.pSwapchains = &g.swapchain;
	present.pImageIndices = &imageIndex;
	VkResult pr = vkQueuePresentKHR(g.graphicsQueue, &present);
	if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
	}
	g.currentFrame++;
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeDestroy(JNIEnv* env, jobject thiz) {
	if (!g.initialized) return;
	std::lock_guard<std::mutex> guard(g_stateMutex);
	vkDeviceWaitIdle(g.device);
	for (size_t i = 0; i < g.imageAvailable.size(); ++i) {
		if (g.imageAvailable[i]) vkDestroySemaphore(g.device, g.imageAvailable[i], nullptr);
		if (g.renderFinished[i]) vkDestroySemaphore(g.device, g.renderFinished[i], nullptr);
		if (g.inFlightFences[i]) vkDestroyFence(g.device, g.inFlightFences[i], nullptr);
	}
	g.imageAvailable.clear();
	g.renderFinished.clear();
	g.inFlightFences.clear();
	destroyTextureResources();
	if (g.commandPool) vkDestroyCommandPool(g.device, g.commandPool, nullptr);
	cleanupSwapchain();
	if (g.device) vkDestroyDevice(g.device, nullptr);
	if (g.surface) vkDestroySurfaceKHR(g.instance, g.surface, nullptr);
	if (g.instance) vkDestroyInstance(g.instance, nullptr);
	if (g.window) ANativeWindow_release(g.window);
	if (g.builder) { delete g.builder; g.builder = nullptr; }
	g = VulkanState{};
	LOGI("Vulkan destroyed");
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateCamera(JNIEnv* env, jobject thiz, jfloat yaw, jfloat pitch, jfloat roll) {
	if (!g.initialized) return;
	if (yaw != 0.0f) g.camera.rotateYaw(static_cast<float>(yaw));
	if (pitch != 0.0f) g.camera.rotatePitch(static_cast<float>(pitch));
	if (roll != 0.0f) g.camera.rotateRoll(static_cast<float>(roll));
}


