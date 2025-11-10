#include "vkrenderer.h"
#include <android/native_window_jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <cstring>
#include "Model.h"

#define LOG_TAG "VKRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct VulkanState {
	ANativeWindow* window = nullptr;
	AAssetManager* assetManager = nullptr;
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
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexMemory = VK_NULL_HANDLE;
	uint32_t indexCount = 0;
	std::vector<VkFramebuffer> framebuffers;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;
	bool initialized = false;
};

static VulkanState g;

static void check(VkResult r, const char* what) {
	if (r != VK_SUCCESS) {
		LOGE("Vulkan error %d at %s", r, what);
		abort();
	}
}

static void createInstance() {
	if (g.instance != VK_NULL_HANDLE) return;
	VkApplicationInfo appInfo{ };
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VKRenderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.pEngineName = "NoEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	const char* exts[] = {"VK_KHR_surface", "VK_KHR_android_surface"};

	VkInstanceCreateInfo ci{ };
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &appInfo;
	ci.enabledExtensionCount = 2;
	ci.ppEnabledExtensionNames = exts;
	check(vkCreateInstance(&ci, nullptr, &g.instance), "vkCreateInstance");
}

static void createSurface() {
	if (g.surface != VK_NULL_HANDLE) return;
	VkAndroidSurfaceCreateInfoKHR sci{ };
	sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	sci.window = g.window;
	check(vkCreateAndroidSurfaceKHR(g.instance, &sci, nullptr, &g.surface), "vkCreateAndroidSurfaceKHR");
}

static void pickPhysicalDevice() {
	uint32_t count = 0;
	check(vkEnumeratePhysicalDevices(g.instance, &count, nullptr), "vkEnumeratePhysicalDevices(count)");
	std::vector<VkPhysicalDevice> devices(count);
	check(vkEnumeratePhysicalDevices(g.instance, &count, devices.data()), "vkEnumeratePhysicalDevices(list)");

	for (auto d : devices) {
		uint32_t qCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
		std::vector<VkQueueFamilyProperties> qprops(qCount);
		vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qprops.data());
		for (uint32_t i = 0; i < qCount; ++i) {
			VkBool32 presentSupport = VK_FALSE;
			check(vkGetPhysicalDeviceSurfaceSupportKHR(d, i, g.surface, &presentSupport), "vkGetPhysicalDeviceSurfaceSupportKHR");
			if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
				g.physicalDevice = d;
				g.graphicsQueueFamily = i;
				return;
			}
		}
	}
	abort();
}

static void createDeviceAndQueue() {
	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci{ };
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = g.graphicsQueueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &priority;

	const char* exts[] = {"VK_KHR_swapchain"};

	VkDeviceCreateInfo dci{ };
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = exts;
	check(vkCreateDevice(g.physicalDevice, &dci, nullptr, &g.device), "vkCreateDevice");
	vkGetDeviceQueue(g.device, g.graphicsQueueFamily, 0, &g.graphicsQueue);
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
	for (auto& f : formats) {
		if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
	}
	return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
	for (auto m : modes) {
		if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

static void createSwapchain(uint32_t width, uint32_t height) {
	if (g.swapchain != VK_NULL_HANDLE) return;
	VkSurfaceCapabilitiesKHR caps{};
	check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g.physicalDevice, g.surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

	uint32_t formatCount = 0;
	check(vkGetPhysicalDeviceSurfaceFormatsKHR(g.physicalDevice, g.surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	check(vkGetPhysicalDeviceSurfaceFormatsKHR(g.physicalDevice, g.surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
	auto chosenFormat = chooseSurfaceFormat(formats);

	uint32_t presentCount = 0;
	check(vkGetPhysicalDeviceSurfacePresentModesKHR(g.physicalDevice, g.surface, &presentCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
	std::vector<VkPresentModeKHR> presentModes(presentCount);
	check(vkGetPhysicalDeviceSurfacePresentModesKHR(g.physicalDevice, g.surface, &presentCount, presentModes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
	auto chosenPresent = choosePresentMode(presentModes);

	VkExtent2D extent = { width, height };
	if (caps.currentExtent.width != UINT32_MAX) {
		extent = caps.currentExtent;
	}

	uint32_t imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR sci{ };
	sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sci.surface = g.surface;
	sci.minImageCount = imageCount;
	sci.imageFormat = chosenFormat.format;
	sci.imageColorSpace = chosenFormat.colorSpace;
	sci.imageExtent = extent;
	sci.imageArrayLayers = 1;
	sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sci.preTransform = caps.currentTransform;
	sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sci.presentMode = chosenPresent;
	sci.clipped = VK_TRUE;
	sci.oldSwapchain = VK_NULL_HANDLE;
	check(vkCreateSwapchainKHR(g.device, &sci, nullptr, &g.swapchain), "vkCreateSwapchainKHR");

	uint32_t scImageCount = 0;
	check(vkGetSwapchainImagesKHR(g.device, g.swapchain, &scImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
	g.swapchainImages.resize(scImageCount);
	check(vkGetSwapchainImagesKHR(g.device, g.swapchain, &scImageCount, g.swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");
	g.swapchainFormat = chosenFormat.format;
	g.swapchainExtent = extent;
}

static void createImageViews() {
	g.swapchainImageViews.resize(g.swapchainImages.size());
	for (size_t i = 0; i < g.swapchainImages.size(); ++i) {
		VkImageViewCreateInfo ci{ };
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.image = g.swapchainImages[i];
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = g.swapchainFormat;
		ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ci.subresourceRange.baseMipLevel = 0;
		ci.subresourceRange.levelCount = 1;
		ci.subresourceRange.baseArrayLayer = 0;
		ci.subresourceRange.layerCount = 1;
		check(vkCreateImageView(g.device, &ci, nullptr, &g.swapchainImageViews[i]), "vkCreateImageView");
	}
}

static void createRenderPass() {
	VkAttachmentDescription color{};
	color.format = g.swapchainFormat;
	color.samples = VK_SAMPLE_COUNT_1_BIT;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = 1;
	rpci.pAttachments = &color;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpass;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;
	check(vkCreateRenderPass(g.device, &rpci, nullptr, &g.renderPass), "vkCreateRenderPass");
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

static void uploadModelBuffers() {
	Model cube = createCubeModel();
	g.indexCount = static_cast<uint32_t>(cube.indices.size());

	VkDeviceSize vsize = sizeof(float) * cube.positions.size();
	VkDeviceSize isize = sizeof(uint16_t) * cube.indices.size();

	createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g.vertexBuffer, g.vertexMemory);
	createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g.indexBuffer, g.indexMemory);

	void* data = nullptr;
	check(vkMapMemory(g.device, g.vertexMemory, 0, vsize, 0, &data), "vkMapMemory(vertex)");
	memcpy(data, cube.positions.data(), (size_t)vsize);
	vkUnmapMemory(g.device, g.vertexMemory);

	check(vkMapMemory(g.device, g.indexMemory, 0, isize, 0, &data), "vkMapMemory(index)");
	memcpy(data, cube.indices.data(), (size_t)isize);
	vkUnmapMemory(g.device, g.indexMemory);
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

static VkShaderModule createShaderModule(const uint32_t* code, size_t wordCount) {
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = wordCount * sizeof(uint32_t);
	ci.pCode = code;
	VkShaderModule module = VK_NULL_HANDLE;
	check(vkCreateShaderModule(g.device, &ci, nullptr, &module), "vkCreateShaderModule");
	return module;
}

static void createPipeline() {
	auto vertSpv = loadSpirvFromAsset("shaders/triangle.vert.spv");
	auto fragSpv = loadSpirvFromAsset("shaders/triangle.frag.spv");
	if (vertSpv.empty() || fragSpv.empty()) {
		LOGE("Failed to load SPIR-V from assets");
		return;
	}
	VkShaderModule vert = createShaderModule(vertSpv.data(), vertSpv.size());
	VkShaderModule frag = createShaderModule(fragSpv.data(), fragSpv.size());

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = frag;
	stages[1].pName = "main";

	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(float) * 3;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attr{};
	attr.location = 0;
	attr.binding = 0;
	attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	attr.offset = 0;
	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = 1;
	vertexInput.pVertexAttributeDescriptions = &attr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo raster{};
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.depthClampEnable = VK_FALSE;
	raster.rasterizerDiscardEnable = VK_FALSE;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.depthBiasEnable = VK_FALSE;
	raster.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlend{};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &colorBlendAttachment;

	VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamics;

	VkPipelineLayoutCreateInfo layoutCi{};
	layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	check(vkCreatePipelineLayout(g.device, &layoutCi, nullptr, &g.pipelineLayout), "vkCreatePipelineLayout");

	VkGraphicsPipelineCreateInfo gpci{};
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.stageCount = 2;
	gpci.pStages = stages;
	gpci.pVertexInputState = &vertexInput;
	gpci.pInputAssemblyState = &inputAssembly;
	gpci.pViewportState = &viewportState;
	gpci.pRasterizationState = &raster;
	gpci.pMultisampleState = &multisample;
	gpci.pDepthStencilState = nullptr;
	gpci.pColorBlendState = &colorBlend;
	gpci.pDynamicState = &dynamicState;
	gpci.layout = g.pipelineLayout;
	gpci.renderPass = g.renderPass;
	gpci.subpass = 0;
	check(vkCreateGraphicsPipelines(g.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &g.graphicsPipeline), "vkCreateGraphicsPipelines");

	vkDestroyShaderModule(g.device, vert, nullptr);
	vkDestroyShaderModule(g.device, frag, nullptr);
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
		// Dynamic viewport/scissor and draw
		VkViewport vp{};
		vp.x = 0.0f; vp.y = 0.0f;
		vp.width = static_cast<float>(g.swapchainExtent.width);
		vp.height = static_cast<float>(g.swapchainExtent.height);
		vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
		VkRect2D sc{};
		sc.offset = {0,0};
		sc.extent = g.swapchainExtent;
		vkCmdSetViewport(g.commandBuffers[i], 0, 1, &vp);
		vkCmdSetScissor(g.commandBuffers[i], 0, 1, &sc);
		vkCmdBindPipeline(g.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g.graphicsPipeline);
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(g.commandBuffers[i], 0, 1, &g.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(g.commandBuffers[i], g.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(g.commandBuffers[i], g.indexCount, 1, 0, 0, 0);
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
	if (g.indexBuffer) { vkDestroyBuffer(g.device, g.indexBuffer, nullptr); g.indexBuffer = VK_NULL_HANDLE; }
	if (g.indexMemory) { vkFreeMemory(g.device, g.indexMemory, nullptr); g.indexMemory = VK_NULL_HANDLE; }
	if (g.vertexBuffer) { vkDestroyBuffer(g.device, g.vertexBuffer, nullptr); g.vertexBuffer = VK_NULL_HANDLE; }
	if (g.vertexMemory) { vkFreeMemory(g.device, g.vertexMemory, nullptr); g.vertexMemory = VK_NULL_HANDLE; }
	if (g.graphicsPipeline) { vkDestroyPipeline(g.device, g.graphicsPipeline, nullptr); g.graphicsPipeline = VK_NULL_HANDLE; }
	if (g.pipelineLayout) { vkDestroyPipelineLayout(g.device, g.pipelineLayout, nullptr); g.pipelineLayout = VK_NULL_HANDLE; }
	for (auto f : g.framebuffers) if (f) vkDestroyFramebuffer(g.device, f, nullptr);
	g.framebuffers.clear();
	if (g.renderPass) { vkDestroyRenderPass(g.device, g.renderPass, nullptr); g.renderPass = VK_NULL_HANDLE; }
	for (auto iv : g.swapchainImageViews) if (iv) vkDestroyImageView(g.device, iv, nullptr);
	g.swapchainImageViews.clear();
	if (g.swapchain) { vkDestroySwapchainKHR(g.device, g.swapchain, nullptr); g.swapchain = VK_NULL_HANDLE; }
}

static void recreateSwapchain(uint32_t width, uint32_t height) {
	vkDeviceWaitIdle(g.device);
	cleanupSwapchain();
	createSwapchain(width, height);
	createImageViews();
	createRenderPass();
	createPipeline();
	uploadModelBuffers();
	createFramebuffers();
	createCommandPoolBuffers();
	recordCommandBuffers();
	createSyncObjects();
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject assetManager) {
	g.window = ANativeWindow_fromSurface(env, surface);
	g.assetManager = AAssetManager_fromJava(env, assetManager);
	uint32_t width = static_cast<uint32_t>(ANativeWindow_getWidth(g.window));
	uint32_t height = static_cast<uint32_t>(ANativeWindow_getHeight(g.window));

	createInstance();
	createSurface();
	pickPhysicalDevice();
	createDeviceAndQueue();
	recreateSwapchain(width, height);
	g.initialized = true;
	LOGI("Vulkan initialized w=%u h=%u", width, height);
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height) {
	if (!g.initialized) return;
	if (width <= 0 || height <= 0) return;
	recreateSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeRender(JNIEnv* env, jobject thiz) {
	if (!g.initialized) return;
	size_t i = g.currentFrame % g.commandBuffers.size();
	check(vkWaitForFences(g.device, 1, &g.inFlightFences[i], VK_TRUE, UINT64_MAX), "vkWaitForFences");
	check(vkResetFences(g.device, 1, &g.inFlightFences[i]), "vkResetFences");

	uint32_t imageIndex;
	VkResult acq = vkAcquireNextImageKHR(g.device, g.swapchain, UINT64_MAX, g.imageAvailable[i], VK_NULL_HANDLE, &imageIndex);
	if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
		return;
	}
	check(acq, "vkAcquireNextImageKHR");

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
	check(vkQueueSubmit(g.graphicsQueue, 1, &submit, g.inFlightFences[i]), "vkQueueSubmit");

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
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeDestroy(JNIEnv* env, jobject thiz) {
	if (!g.initialized) return;
	vkDeviceWaitIdle(g.device);
	for (size_t i = 0; i < g.imageAvailable.size(); ++i) {
		if (g.imageAvailable[i]) vkDestroySemaphore(g.device, g.imageAvailable[i], nullptr);
		if (g.renderFinished[i]) vkDestroySemaphore(g.device, g.renderFinished[i], nullptr);
		if (g.inFlightFences[i]) vkDestroyFence(g.device, g.inFlightFences[i], nullptr);
	}
	g.imageAvailable.clear();
	g.renderFinished.clear();
	g.inFlightFences.clear();
	if (g.commandPool) vkDestroyCommandPool(g.device, g.commandPool, nullptr);
	cleanupSwapchain();
	if (g.device) vkDestroyDevice(g.device, nullptr);
	if (g.surface) vkDestroySurfaceKHR(g.instance, g.surface, nullptr);
	if (g.instance) vkDestroyInstance(g.instance, nullptr);
	if (g.window) ANativeWindow_release(g.window);
	g = VulkanState{};
	LOGI("Vulkan destroyed");
}


