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
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeLoadModel(JNIEnv* env, jobject thiz, jlong modelId, jstring modelName, jfloat x, jfloat y, jfloat z, jfloat scale);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeMoveCamera(JNIEnv* env, jobject thiz, jfloat delta);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeSetCameraPosition(JNIEnv* env, jobject thiz, jfloat x, jfloat y, jfloat z);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateModel(JNIEnv* env, jobject thiz, jlong modelId, jfloat rotationX, jfloat rotationY, jfloat rotationZ);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeTranslateModel(JNIEnv* env, jobject thiz, jlong modelId, jfloat x, jfloat y, jfloat z);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeScaleModel(JNIEnv* env, jobject thiz, jlong modelId, jfloat scale);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRender(JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeDestroy(JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateCamera(JNIEnv* env, jobject thiz, jfloat yaw, jfloat pitch, jfloat roll);

JNIEXPORT void JNICALL
Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeSetCameraRotation(JNIEnv* env, jobject thiz, jfloat yaw, jfloat pitch, jfloat roll);

#ifdef __cplusplus
}
#endif


