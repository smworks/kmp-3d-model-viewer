#include "engine.h"

#include <android/native_window_jni.h>
#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <memory>
#include <string>

#include "GraphicsEngine.h"
#include "engine/EngineCore.h"

#define LOG_TAG "EngineNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace
{
	std::unique_ptr<engine::CEngineCore> g_pEngineCore;
	jclass g_engineApiGlobalClass = nullptr;
	jmethodID g_loadFileGlobalMethod = nullptr;
	JavaVM* g_javaVmGlobal = nullptr;
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject assetManager)
	{
		if (!g_javaVmGlobal)
		{
			env->GetJavaVM(&g_javaVmGlobal);
		}
		if (!g_engineApiGlobalClass)
		{
			jclass localClass = env->GetObjectClass(thiz);
			if (localClass)
			{
				g_engineApiGlobalClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
				env->DeleteLocalRef(localClass);
			}
		}
		if (g_engineApiGlobalClass && !g_loadFileGlobalMethod)
		{
			g_loadFileGlobalMethod = env->GetStaticMethodID(g_engineApiGlobalClass, "loadFile", "(Ljava/lang/String;)[B");
			if (!g_loadFileGlobalMethod)
			{
				LOGE("Failed to locate EngineAPI.loadFile");
			}
		}
		engine::SetJavaBindings(g_javaVmGlobal, g_engineApiGlobalClass, g_loadFileGlobalMethod);

		ANativeWindow* pWindow = ANativeWindow_fromSurface(env, surface);
		AAssetManager* pAssetManager = AAssetManager_fromJava(env, assetManager);
		if (!pWindow || !pAssetManager)
		{
			LOGE("nativeInit received invalid window or asset manager");
			return;
		}

		if (!g_pEngineCore)
		{
			g_pEngineCore = std::make_unique<engine::CEngineCore>(std::make_unique<engine::CGraphicsEngine>());
		}
		engine::SInitConfig sConfig;
		sConfig.pNativeWindow = pWindow;
		sConfig.pPlatformHandle = pAssetManager;
		sConfig.uWidth = static_cast<uint32_t>(ANativeWindow_getWidth(pWindow));
		sConfig.uHeight = static_cast<uint32_t>(ANativeWindow_getHeight(pWindow));

		if (!g_pEngineCore->bInitialize(sConfig))
		{
			LOGE("Engine core initialization failed");
		}
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeResize(JNIEnv*, jobject, jint width, jint height)
	{
		if (!g_pEngineCore) return;
		if (width <= 0 || height <= 0) return;
		engine::SFrameConfig sFrame;
		sFrame.uWidth = static_cast<uint32_t>(width);
		sFrame.uHeight = static_cast<uint32_t>(height);
		g_pEngineCore->Resize(sFrame);
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeLoadModel(JNIEnv* env, jobject, jlong modelId, jstring modelName, jfloat x, jfloat y, jfloat z, jfloat scale)
	{
		if (!g_pEngineCore) return;
		const char* pcModelChars = modelName ? env->GetStringUTFChars(modelName, nullptr) : nullptr;
		std::string strModelPath = pcModelChars ? std::string(pcModelChars) : std::string();
		if (pcModelChars)
		{
			env->ReleaseStringUTFChars(modelName, pcModelChars);
		}

		if (strModelPath.empty())
		{
			LOGE("nativeLoadModel called with empty model path");
			return;
		}

		engine::SModelDescription sDesc;
		sDesc.llId = static_cast<int64_t>(modelId);
		sDesc.sAssetPath = std::move(strModelPath);
		sDesc.sTransform.fPosition[0] = static_cast<float>(x);
		sDesc.sTransform.fPosition[1] = static_cast<float>(y);
		sDesc.sTransform.fPosition[2] = static_cast<float>(z);
		sDesc.sTransform.fScale = scale > 0.0f ? static_cast<float>(scale) : 1.0f;
		g_pEngineCore->LoadModel(sDesc);
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeMoveCamera(JNIEnv*, jobject, jfloat delta)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->MoveCamera(static_cast<float>(delta));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeSetCameraPosition(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->SetCameraPosition(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateModel(JNIEnv*, jobject, jlong modelId, jfloat rotationX, jfloat rotationY, jfloat rotationZ)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->RotateModel(static_cast<int64_t>(modelId),
			static_cast<float>(rotationX),
			static_cast<float>(rotationY),
			static_cast<float>(rotationZ));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeTranslateModel(JNIEnv*, jobject, jlong modelId, jfloat x, jfloat y, jfloat z)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->TranslateModel(static_cast<int64_t>(modelId),
			static_cast<float>(x),
			static_cast<float>(y),
			static_cast<float>(z));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeScaleModel(JNIEnv*, jobject, jlong modelId, jfloat scale)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->ScaleModel(static_cast<int64_t>(modelId), static_cast<float>(scale));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRender(JNIEnv*, jobject)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->Render();
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeDestroy(JNIEnv*, jobject)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->Shutdown();
		g_pEngineCore.reset();
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeRotateCamera(JNIEnv*, jobject, jfloat yaw, jfloat pitch, jfloat roll)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->RotateCamera(static_cast<float>(yaw), static_cast<float>(pitch), static_cast<float>(roll));
	}

	JNIEXPORT void JNICALL
	Java_lt_smworks_multiplatform3dengine_vulkan_EngineAPI_nativeSetCameraRotation(JNIEnv*, jobject, jfloat yaw, jfloat pitch, jfloat roll)
	{
		if (!g_pEngineCore) return;
		g_pEngineCore->SetCameraRotation(static_cast<float>(yaw), static_cast<float>(pitch), static_cast<float>(roll));
	}
}


