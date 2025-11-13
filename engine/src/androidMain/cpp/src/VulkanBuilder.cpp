#include "VulkanBuilder.h"
#include <android/log.h>

#define LOG_TAG "VKBuilder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void VulkanBuilder::check(VkResult r, const char* what) {
	if (r != VK_SUCCESS) {
		LOGE("Vulkan error %d at %s", r, what);
		abort();
	}
}

VulkanBuilder::VulkanBuilder(ANativeWindow* window, AAssetManager* assets)
	: window(window), assets(assets) {}

VulkanBuilder& VulkanBuilder::buildInstance() {
	if (instance) return *this;
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
	check(vkCreateInstance(&ci, nullptr, &instance), "vkCreateInstance");
	return *this;
}

VulkanBuilder& VulkanBuilder::buildSurface() {
	if (surface) return *this;
	VkAndroidSurfaceCreateInfoKHR sci{ };
	sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	sci.window = window;
	check(vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, &surface), "vkCreateAndroidSurfaceKHR");
	return *this;
}

VulkanBuilder& VulkanBuilder::pickPhysicalDevice() {
	uint32_t count = 0;
	check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "vkEnumeratePhysicalDevices(count)");
	std::vector<VkPhysicalDevice> devices(count);
	check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "vkEnumeratePhysicalDevices(list)");
	for (auto d : devices) {
		uint32_t qCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
		std::vector<VkQueueFamilyProperties> qprops(qCount);
		vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qprops.data());
		for (uint32_t i = 0; i < qCount; ++i) {
			VkBool32 presentSupport = VK_FALSE;
			check(vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &presentSupport), "vkGetPhysicalDeviceSurfaceSupportKHR");
			if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
				physicalDevice = d;
				graphicsQueueFamily = i;
				return *this;
			}
		}
	}
	LOGE("No suitable physical device found");
	abort();
}

VulkanBuilder& VulkanBuilder::buildDeviceAndQueue() {
	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci{ };
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = graphicsQueueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &priority;

	const char* exts[] = {"VK_KHR_swapchain"};

	VkDeviceCreateInfo dci{ };
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = exts;
	check(vkCreateDevice(physicalDevice, &dci, nullptr, &device), "vkCreateDevice");
	vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
	return *this;
}

VulkanBuilder& VulkanBuilder::buildSwapchain(uint32_t width, uint32_t height) {
	VkSurfaceCapabilitiesKHR caps{};
	check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

	uint32_t formatCount = 0;
	check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
	VkSurfaceFormatKHR chosenFormat = formats[0];
	for (auto& f : formats) {
		if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosenFormat = f; break; }
	}

	uint32_t presentCount = 0;
	check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
	std::vector<VkPresentModeKHR> presentModes(presentCount);
	check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
	VkPresentModeKHR chosenPresent = VK_PRESENT_MODE_FIFO_KHR;
	for (auto m : presentModes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) { chosenPresent = m; break; }

	VkExtent2D extent = { width, height };
	if (caps.currentExtent.width != UINT32_MAX) extent = caps.currentExtent;

	uint32_t imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR sci{ };
	sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sci.surface = surface;
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
	check(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain), "vkCreateSwapchainKHR");

	uint32_t scImageCount = 0;
	check(vkGetSwapchainImagesKHR(device, swapchain, &scImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
	swapchainImages.resize(scImageCount);
	check(vkGetSwapchainImagesKHR(device, swapchain, &scImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");
	swapchainFormat = chosenFormat.format;
	swapchainExtent = extent;
	return *this;
}

VulkanBuilder& VulkanBuilder::buildImageViews() {
	swapchainImageViews.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); ++i) {
		VkImageViewCreateInfo ci{ };
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.image = swapchainImages[i];
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = swapchainFormat;
		ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ci.subresourceRange.baseMipLevel = 0;
		ci.subresourceRange.levelCount = 1;
		ci.subresourceRange.baseArrayLayer = 0;
		ci.subresourceRange.layerCount = 1;
		check(vkCreateImageView(device, &ci, nullptr, &swapchainImageViews[i]), "vkCreateImageView");
	}
	return *this;
}

VulkanBuilder& VulkanBuilder::buildRenderPass() {
	VkAttachmentDescription color{};
	color.format = swapchainFormat;
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
	check(vkCreateRenderPass(device, &rpci, nullptr, &renderPass), "vkCreateRenderPass");
	return *this;
}

VulkanBuilder& VulkanBuilder::setVertexSpirv(const std::vector<uint32_t>& vert) {
	vertSpv = vert;
	return *this;
}

VulkanBuilder& VulkanBuilder::setFragmentSpirv(const std::vector<uint32_t>& frag) {
	fragSpv = frag;
	return *this;
}

VulkanBuilder& VulkanBuilder::buildPipeline() {
	auto createShaderModule = [&](const std::vector<uint32_t>& code, VkShaderModule& out) {
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = code.size() * sizeof(uint32_t);
		ci.pCode = code.data();
		check(vkCreateShaderModule(device, &ci, nullptr, &out), "vkCreateShaderModule");
	};

	VkShaderModule vertModule = VK_NULL_HANDLE;
	VkShaderModule fragModule = VK_NULL_HANDLE;
	createShaderModule(vertSpv, vertModule);
	createShaderModule(fragSpv, fragModule);

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragModule;
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

	// Push constants for camera rotation, viewport size, and distance (6 floats)
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float) * 6;
	
	VkPipelineLayoutCreateInfo layoutCi{};
	layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCi.pushConstantRangeCount = 1;
	layoutCi.pPushConstantRanges = &pushConstantRange;
	check(vkCreatePipelineLayout(device, &layoutCi, nullptr, &pipelineLayout), "vkCreatePipelineLayout");

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
	gpci.layout = pipelineLayout;
	gpci.renderPass = renderPass;
	gpci.subpass = 0;
	check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &graphicsPipeline), "vkCreateGraphicsPipelines");

	vkDestroyShaderModule(device, vertModule, nullptr);
	vkDestroyShaderModule(device, fragModule, nullptr);
	return *this;
}

void VulkanBuilder::cleanupSwapchain() {
	if (graphicsPipeline) { vkDestroyPipeline(device, graphicsPipeline, nullptr); graphicsPipeline = VK_NULL_HANDLE; }
	if (pipelineLayout) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); pipelineLayout = VK_NULL_HANDLE; }
	for (auto iv : swapchainImageViews) if (iv) vkDestroyImageView(device, iv, nullptr);
	swapchainImageViews.clear();
	if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = VK_NULL_HANDLE; }
	if (swapchain) { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = VK_NULL_HANDLE; }
}


