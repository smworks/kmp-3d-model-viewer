#include "Camera.h"

Camera::Camera() {
	// Initialize with default values
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = 0.0f;
	viewport.height = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	
	scissor.offset = {0, 0};
	scissor.extent = {0, 0};
}

void Camera::updateViewport(const VkExtent2D& extent) {
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	
	scissor.offset = {0, 0};
	scissor.extent = extent;
}

void Camera::applyToCommandBuffer(VkCommandBuffer commandBuffer) const {
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

