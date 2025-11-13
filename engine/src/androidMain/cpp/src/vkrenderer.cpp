#include "vkrenderer.h"
#include <android/native_window_jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <cstring>
#include "ModelLoader.h"
#include "VulkanBuilder.h"
#include "Camera.h"

#define LOG_TAG "VKRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
	Camera camera;
	bool modelLoaded = false;
	float modelOffset[3] = {0.0f, 0.0f, 0.0f};
};

static VulkanState g;

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

static void destroyModelBuffers() {
	if (g.indexBuffer) { vkDestroyBuffer(g.device, g.indexBuffer, nullptr); g.indexBuffer = VK_NULL_HANDLE; }
	if (g.indexMemory) { vkFreeMemory(g.device, g.indexMemory, nullptr); g.indexMemory = VK_NULL_HANDLE; }
	if (g.vertexBuffer) { vkDestroyBuffer(g.device, g.vertexBuffer, nullptr); g.vertexBuffer = VK_NULL_HANDLE; }
	if (g.vertexMemory) { vkFreeMemory(g.device, g.vertexMemory, nullptr); g.vertexMemory = VK_NULL_HANDLE; }
	g.indexCount = 0;
}

static void uploadModelBuffers(float x, float y, float z) {
	Model model = loadModel();
	g.indexCount = static_cast<uint32_t>(model.indices.size());
	g.modelOffset[0] = x;
	g.modelOffset[1] = y;
	g.modelOffset[2] = z;

	VkDeviceSize vsize = sizeof(float) * model.positions.size();
	VkDeviceSize isize = sizeof(uint16_t) * model.indices.size();

	createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g.vertexBuffer, g.vertexMemory);
	createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g.indexBuffer, g.indexMemory);

	void* data = nullptr;
	check(vkMapMemory(g.device, g.vertexMemory, 0, vsize, 0, &data), "vkMapMemory(vertex)");
	memcpy(data, model.positions.data(), (size_t)vsize);
	vkUnmapMemory(g.device, g.vertexMemory);

	check(vkMapMemory(g.device, g.indexMemory, 0, isize, 0, &data), "vkMapMemory(index)");
	memcpy(data, model.indices.data(), (size_t)isize);
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

static void buildPipelineWithBuilder() {
	auto vert = loadSpirvFromAsset("shaders/triangle.vert.spv");
	auto frag = loadSpirvFromAsset("shaders/triangle.frag.spv");
	g.builder->setVertexSpirv(vert)
		.setFragmentSpirv(frag)
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
		if (g.indexCount > 0 && g.vertexBuffer && g.indexBuffer) {
			float pushConstants[9] = {
				g.camera.getYaw(),
				g.camera.getPitch(),
				g.camera.getRoll(),
				g.camera.getWidth(),
				g.camera.getHeight(),
				g.camera.getDistance(),
				g.modelOffset[0],
				g.modelOffset[1],
				g.modelOffset[2]
			};
			vkCmdPushConstants(g.commandBuffers[i], g.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), pushConstants);
			
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(g.commandBuffers[i], 0, 1, &g.vertexBuffer, offsets);
			vkCmdBindIndexBuffer(g.commandBuffers[i], g.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(g.commandBuffers[i], g.indexCount, 1, 0, 0, 0);
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
	destroyModelBuffers();
	g.builder->cleanupSwapchain();
	for (auto f : g.framebuffers) if (f) vkDestroyFramebuffer(g.device, f, nullptr);
	g.framebuffers.clear();
}

static void recreateSwapchain(uint32_t width, uint32_t height) {
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
	if (g.modelLoaded) {
		uploadModelBuffers(g.modelOffset[0], g.modelOffset[1], g.modelOffset[2]);
	}
	createFramebuffers();
	createCommandPoolBuffers();
	recordCommandBuffers();
	createSyncObjects();
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject assetManager) {
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
	if (g.modelLoaded) {
		uploadModelBuffers(g.modelOffset[0], g.modelOffset[1], g.modelOffset[2]);
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
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeLoadModel(JNIEnv* env, jobject thiz, jfloat x, jfloat y, jfloat z) {
	if (!g.initialized) return;
	vkDeviceWaitIdle(g.device);
	destroyModelBuffers();
	uploadModelBuffers(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
	g.modelLoaded = true;
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeMoveCamera(JNIEnv* env, jobject thiz, jfloat delta) {
	if (!g.initialized) return;
	g.camera.move(static_cast<float>(delta));
}

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRender(JNIEnv* env, jobject thiz) {
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
	if (g.indexCount > 0 && g.vertexBuffer && g.indexBuffer) {
		float pushConstants[9] = {
			g.camera.getYaw(),
			g.camera.getPitch(),
			g.camera.getRoll(),
			g.camera.getWidth(),
			g.camera.getHeight(),
			g.camera.getDistance(),
			g.modelOffset[0],
			g.modelOffset[1],
			g.modelOffset[2]
		};
		vkCmdPushConstants(g.commandBuffers[imageIndex], g.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), pushConstants);
		
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(g.commandBuffers[imageIndex], 0, 1, &g.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(g.commandBuffers[imageIndex], g.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(g.commandBuffers[imageIndex], g.indexCount, 1, 0, 0, 0);
	}
	vkCmdEndRenderPass(g.commandBuffers[imageIndex]);
	check(vkEndCommandBuffer(g.commandBuffers[imageIndex]), "vkEndCommandBuffer");

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
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeDestroy(JNIEnv* env, jobject thiz) {
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
	LOGI("Camera rotated: yaw=%.3f pitch=%.3f roll=%.3f", g.camera.getYaw(), g.camera.getPitch(), g.camera.getRoll());
}


