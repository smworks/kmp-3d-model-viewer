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
	const VkViewport& getViewport() const { return oViewport; }
	
	// Get scissor configuration
	const VkRect2D& getScissor() const { return oScissor; }
	
	// Get viewport dimensions
	float getWidth() const { return oViewport.width; }
	float getHeight() const { return oViewport.height; }
	float getAspectRatio() const { return oViewport.width > 0 ? oViewport.width / oViewport.height : 1.0f; }
	
	// Apply viewport and scissor to a command buffer
	void applyToCommandBuffer(VkCommandBuffer commandBuffer) const;

	// Rotation methods
	void rotateYaw(float angle);      // Rotate around Y axis (horizontal rotation)
	void rotatePitch(float angle);    // Rotate around X axis (vertical rotation)
	void rotateRoll(float angle);     // Rotate around Z axis (roll rotation)
	
	// Get rotation angles (in radians)
	float getYaw() const { return fYaw; }
	float getPitch() const { return fPitch; }
	float getRoll() const { return fRoll; }
	
	// Set rotation angles directly (in radians)
	void setYaw(float angle);
	void setPitch(float angle);
	void setRoll(float angle);
	void setRotation(float yaw, float pitch, float roll);

	// Position setters/getters
	void setPosition(float x, float y, float z);
	float getPositionX() const { return fPosX; }
	float getPositionY() const { return fPosY; }
	float getPositionZ() const { return fPosZ; }

	// Movement along the forward axis (negative Z in world space)
	void move(float delta);

private:
	VkViewport oViewport{};
	VkRect2D oScissor{};
	
	// Rotation angles in radians
	float fYaw = 0.0f;    // Rotation around Y axis
	float fPitch = 0.0f;  // Rotation around X axis
	float fRoll = 0.0f;   // Rotation around Z axis

	float fPosX = 0.0f;
	float fPosY = 0.0f;
	float fPosZ = 4.0f;

	float clampPitch(float angle) const;
	float wrapAngle(float angle) const;
};

