#pragma once

#include <jni.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeInit(JNIEnv* env, jobject thiz, jobject surface);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeRender(JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_VulkanNativeRenderer_nativeDestroy(JNIEnv* env, jobject thiz);

#ifdef __cplusplus
}
#endif


