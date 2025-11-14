#pragma once

#include <jni.h>
#include <android/native_window.h>
#include <android/asset_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject assetManager);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeLoadModel(JNIEnv* env, jobject thiz, jstring modelName, jfloat x, jfloat y, jfloat z, jfloat scale);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeMoveCamera(JNIEnv* env, jobject thiz, jfloat delta);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRender(JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeDestroy(JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateCamera(JNIEnv* env, jobject thiz, jfloat yaw, jfloat pitch, jfloat roll);

#ifdef __cplusplus
}
#endif


