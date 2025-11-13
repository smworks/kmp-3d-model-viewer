#pragma once

#include <vulkan/vulkan.h>
#include <cmath>

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
	
	// Get viewport dimensions
	float getWidth() const { return viewport.width; }
	float getHeight() const { return viewport.height; }
	float getAspectRatio() const { return viewport.width > 0 ? viewport.width / viewport.height : 1.0f; }
	
	// Apply viewport and scissor to a command buffer
	void applyToCommandBuffer(VkCommandBuffer commandBuffer) const;

	// Rotation methods
	void rotateYaw(float angle);      // Rotate around Y axis (horizontal rotation)
	void rotatePitch(float angle);    // Rotate around X axis (vertical rotation)
	void rotateRoll(float angle);     // Rotate around Z axis (roll rotation)
	
	// Get rotation angles (in radians)
	float getYaw() const { return yaw; }
	float getPitch() const { return pitch; }
	float getRoll() const { return roll; }
	
	// Set rotation angles directly (in radians)
	void setYaw(float angle) { yaw = angle; }
	void setPitch(float angle) { pitch = angle; }
	void setRoll(float angle) { roll = angle; }

private:
	VkViewport viewport{};
	VkRect2D scissor{};
	
	// Rotation angles in radians
	float yaw = 0.0f;    // Rotation around Y axis
	float pitch = 0.0f;  // Rotation around X axis
	float roll = 0.0f;   // Rotation around Z axis
};

