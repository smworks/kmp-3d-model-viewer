#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <android/asset_manager.h>

// VulkanBuilder encapsulates Vulkan setup in easy-to-follow steps.
class VulkanBuilder {
public:
	VulkanBuilder(ANativeWindow* window, AAssetManager* assets);

	// Step-by-step setup
	VulkanBuilder& buildInstance();
	VulkanBuilder& buildSurface();
	VulkanBuilder& pickPhysicalDevice();
	VulkanBuilder& buildDeviceAndQueue();
	VulkanBuilder& buildSwapchain(uint32_t width, uint32_t height);
	VulkanBuilder& buildImageViews();
	VulkanBuilder& buildRenderPass();
	VulkanBuilder& buildPipeline(); // expects SPIR-V loaded via setSpirv

	// Provide SPIR-V code for pipeline creation
	VulkanBuilder& setVertexSpirv(const std::vector<uint32_t>& vert);
	VulkanBuilder& setFragmentSpirv(const std::vector<uint32_t>& frag);

	// Swapchain dependent cleanup (does not destroy device/instance/surface)
	void cleanupSwapchain();

	// Accessors
	VkInstance getInstance() const { return instance; }
	VkSurfaceKHR getSurface() const { return surface; }
	VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
	uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
	VkDevice getDevice() const { return device; }
	VkQueue getGraphicsQueue() const { return graphicsQueue; }
	VkSwapchainKHR getSwapchain() const { return swapchain; }
	VkFormat getSwapchainFormat() const { return swapchainFormat; }
	VkExtent2D getSwapchainExtent() const { return swapchainExtent; }
	const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages; }
	const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews; }
	VkRenderPass getRenderPass() const { return renderPass; }
	VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
	VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }

private:
	void check(VkResult r, const char* what);

	ANativeWindow* window;
	AAssetManager* assets;

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

	std::vector<uint32_t> vertSpv;
	std::vector<uint32_t> fragSpv;
};


