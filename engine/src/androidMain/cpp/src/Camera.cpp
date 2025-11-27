#include "Camera.h"
#include <algorithm>
#include <android/log.h>

namespace {
constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kMaxPitch = kPi / 2.0f - 0.1f; // ~89 degrees
constexpr float kMinCameraZ = 1.0f;
constexpr float kMaxCameraZ = 50.0f;
constexpr const char* kCameraLogTag = "VKRenderer";
}

Camera::Camera() {
	oViewport.x = 0.0f;
	oViewport.y = 0.0f;
	oViewport.width = 0.0f;
	oViewport.height = 0.0f;
	oViewport.minDepth = 0.0f;
	oViewport.maxDepth = 1.0f;
	
	oScissor.offset = {0, 0};
	oScissor.extent = {0, 0};
	
	fYaw = 0.0f;
	fPitch = 0.0f;
	fRoll = 0.0f;
	fPosX = 0.0f;
	fPosY = 0.0f;
	fPosZ = 4.0f;
}

void Camera::updateViewport(const VkExtent2D& extent) {
	oViewport.x = 0.0f;
	oViewport.y = 0.0f;
	oViewport.width = static_cast<float>(extent.width);
	oViewport.height = static_cast<float>(extent.height);
	oViewport.minDepth = 0.0f;
	oViewport.maxDepth = 1.0f;
	
	oScissor.offset = {0, 0};
	oScissor.extent = extent;

	__android_log_print(ANDROID_LOG_INFO, kCameraLogTag, "Camera viewport updated width=%.1f height=%.1f aspect=%.3f",
		oViewport.width, oViewport.height, (oViewport.height > 0.0f) ? oViewport.width / oViewport.height : 0.0f);
}

void Camera::applyToCommandBuffer(VkCommandBuffer commandBuffer) const {
	vkCmdSetViewport(commandBuffer, 0, 1, &oViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &oScissor);
}

void Camera::rotateYaw(float angle) {
	fYaw = wrapAngle(fYaw + angle);
}

void Camera::rotatePitch(float angle) {
	fPitch = clampPitch(fPitch + angle);
}

void Camera::rotateRoll(float angle) {
	fRoll = wrapAngle(fRoll + angle);
}

void Camera::setYaw(float angle) {
	fYaw = wrapAngle(angle);
}

void Camera::setPitch(float angle) {
	fPitch = clampPitch(angle);
}

void Camera::setRoll(float angle) {
	fRoll = wrapAngle(angle);
}

void Camera::setRotation(float yaw, float pitch, float roll) {
	setYaw(yaw);
	setPitch(pitch);
	setRoll(roll);
}

void Camera::setPosition(float x, float y, float z) {
	fPosX = x;
	fPosY = y;
	fPosZ = std::clamp(z, kMinCameraZ, kMaxCameraZ);
}

void Camera::move(float delta) {
	fPosZ = std::clamp(fPosZ - delta, kMinCameraZ, kMaxCameraZ);
}

float Camera::clampPitch(float angle) const {
	if (angle > kMaxPitch) return kMaxPitch;
	if (angle < -kMaxPitch) return -kMaxPitch;
	return angle;
}

float Camera::wrapAngle(float angle) const {
	float wrapped = angle;
	while (wrapped > kTwoPi) wrapped -= kTwoPi;
	while (wrapped < -kTwoPi) wrapped += kTwoPi;
	return wrapped;
}

