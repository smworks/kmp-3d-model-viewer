#pragma once

#include <vulkan/vulkan.h>

// Camera class encapsulates viewport and scissor configuration for rendering.
class Camera {
public:
	Camera();
	
	// Update camera viewport and scissor based on swapchain extent
	void updateViewport(const VkExtent2D& extent);
	
	// Get viewport configuration
	const VkViewport& getViewport() const { return viewport; }
	
	// Get scissor configuration
	const VkRect2D& getScissor() const { return scissor; }
	
	// Apply viewport and scissor to a command buffer
	void applyToCommandBuffer(VkCommandBuffer commandBuffer) const;

private:
	VkViewport viewport{};
	VkRect2D scissor{};
};

