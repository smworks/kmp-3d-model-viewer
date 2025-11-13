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
	
	// Initialize rotation angles
	yaw = 0.0f;
	pitch = 0.0f;
	roll = 0.0f;
	distance = -4.0f;
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

void Camera::rotateYaw(float angle) {
	yaw += angle;
	// Keep yaw in reasonable range to avoid precision issues
	const float twoPi = 2.0f * 3.14159265359f;
	while (yaw > twoPi) yaw -= twoPi;
	while (yaw < -twoPi) yaw += twoPi;
}

void Camera::rotatePitch(float angle) {
	pitch += angle;
	// Clamp pitch to avoid gimbal lock
	const float maxPitch = 3.14159265359f / 2.0f - 0.1f; // ~89 degrees
	if (pitch > maxPitch) pitch = maxPitch;
	if (pitch < -maxPitch) pitch = -maxPitch;
}

void Camera::rotateRoll(float angle) {
	roll += angle;
	// Keep roll in reasonable range
	const float twoPi = 2.0f * 3.14159265359f;
	while (roll > twoPi) roll -= twoPi;
	while (roll < -twoPi) roll += twoPi;
}

void Camera::move(float delta) {
	distance += delta;
	// Prevent the camera from crossing the origin or moving too far away.
	const float minDistance = -50.0f;
	const float maxDistance = -1.0f;
	if (distance < minDistance) distance = minDistance;
	if (distance > maxDistance) distance = maxDistance;
}

