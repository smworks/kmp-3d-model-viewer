#include "ImageLoader.h"

#include <android/log.h>
#include <android/imagedecoder.h>
#include <android/bitmap.h>
#include <jni.h>

#include <algorithm>
#include <cstring>
#include <vector>

#define LOG_TAG "ImageLoader"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern JavaVM* g_javaVm;
extern jclass g_engineApiClass;
extern jmethodID g_decodeTextureMethod;

namespace {

JNIEnv* getEnv(bool& didAttach) {
	didAttach = false;
	if (!g_javaVm) return nullptr;
	JNIEnv* env = nullptr;
	jint status = g_javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (status == JNI_EDETACHED) {
		if (g_javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
			return nullptr;
		}
		didAttach = true;
	} else if (status != JNI_OK) {
		return nullptr;
	}
	return env;
}

void releaseEnv(bool didAttach) {
	if (didAttach && g_javaVm) {
		g_javaVm->DetachCurrentThread();
	}
}

bool decodeTextureWithJava(const std::string& assetPath,
	std::vector<unsigned char>& outPixels,
	int& width,
	int& height) {
	if (!g_engineApiClass || !g_decodeTextureMethod) {
		return false;
	}
	bool didAttach = false;
	JNIEnv* env = getEnv(didAttach);
	if (!env) {
		releaseEnv(didAttach);
		return false;
	}

	jstring jPath = env->NewStringUTF(assetPath.c_str());
	if (!jPath) {
		releaseEnv(didAttach);
		return false;
	}

	jbyteArray result = static_cast<jbyteArray>(env->CallStaticObjectMethod(g_engineApiClass, g_decodeTextureMethod, jPath));
	env->DeleteLocalRef(jPath);

	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		if (result) env->DeleteLocalRef(result);
		releaseEnv(didAttach);
		return false;
	}

	if (!result) {
		releaseEnv(didAttach);
		return false;
	}

	jsize len = env->GetArrayLength(result);
	if (len < 8) {
		env->DeleteLocalRef(result);
		releaseEnv(didAttach);
		return false;
	}

	jbyte* data = env->GetByteArrayElements(result, nullptr);
	if (!data) {
		env->DeleteLocalRef(result);
		releaseEnv(didAttach);
		return false;
	}

	std::memcpy(&width, data, sizeof(int));
	std::memcpy(&height, data + 4, sizeof(int));
	size_t pixelBytes = static_cast<size_t>(len) - 8;
	const size_t expectedBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	if (expectedBytes == 0 || pixelBytes < expectedBytes) {
		env->ReleaseByteArrayElements(result, data, JNI_ABORT);
		env->DeleteLocalRef(result);
		releaseEnv(didAttach);
		return false;
	}
	outPixels.resize(expectedBytes);
	if (expectedBytes > 0) {
		std::memcpy(outPixels.data(), data + 8, expectedBytes);
	}

	env->ReleaseByteArrayElements(result, data, JNI_ABORT);
	env->DeleteLocalRef(result);
	releaseEnv(didAttach);

	return width > 0 && height > 0 && !outPixels.empty();
}

} // namespace

bool LoadImageFromAssets(AAssetManager* assetManager,
	const std::string& assetPath,
	int desiredChannels,
	std::vector<unsigned char>& outPixels,
	int& width,
	int& height) {
	outPixels.clear();
	width = 0;
	height = 0;
	if (!assetManager) {
		LOGE("LoadImageFromAssets called with null asset manager");
		return false;
	}
	AAsset* asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_STREAMING);
	if (!asset) {
		LOGE("Failed to open image asset: %s", assetPath.c_str());
		return false;
	}

#if __ANDROID_API__ >= 30
	AImageDecoder* decoder = nullptr;
	const int result = AImageDecoder_createFromAAsset(asset, &decoder);
	if (result != ANDROID_IMAGE_DECODER_SUCCESS || !decoder) {
		LOGE("AImageDecoder failed to create decoder for %s (error %d)", assetPath.c_str(), result);
		AAsset_close(asset);
		if (!decodeTextureWithJava(assetPath, outPixels, width, height)) {
			LOGE("Java fallback failed to decode %s", assetPath.c_str());
			return false;
		}
		return true;
	}

	const AImageDecoderHeaderInfo* header = AImageDecoder_getHeaderInfo(decoder);
	width = AImageDecoderHeaderInfo_getWidth(header);
	height = AImageDecoderHeaderInfo_getHeight(header);
	const AndroidBitmapFormat format = desiredChannels == 4 ? ANDROID_BITMAP_FORMAT_RGBA_8888 : ANDROID_BITMAP_FORMAT_RGB_565;
	AImageDecoder_setAndroidBitmapFormat(decoder, format);

	const size_t minStride = AImageDecoder_getMinimumStride(decoder);
	const size_t expectedStride = static_cast<size_t>(width) * static_cast<size_t>(desiredChannels);
	const size_t decodeStride = std::max(minStride, expectedStride);
	const size_t decodeSize = decodeStride * static_cast<size_t>(height);

	std::vector<unsigned char> tempPixels(decodeSize);
	const int decodeResult = AImageDecoder_decodeImage(decoder, tempPixels.data(), decodeStride, tempPixels.size());
	AImageDecoder_delete(decoder);
	AAsset_close(asset);
	if (decodeResult != ANDROID_IMAGE_DECODER_SUCCESS) {
		LOGE("AImageDecoder failed to decode %s (error %d)", assetPath.c_str(), decodeResult);
		if (!decodeTextureWithJava(assetPath, outPixels, width, height)) {
			LOGE("Java fallback failed to decode %s", assetPath.c_str());
			return false;
		}
		return true;
	}

	outPixels.resize(expectedStride * static_cast<size_t>(height));
	if (decodeStride == expectedStride) {
		outPixels = std::move(tempPixels);
	} else {
		for (int row = 0; row < height; ++row) {
			std::memcpy(
				outPixels.data() + static_cast<size_t>(row) * expectedStride,
				tempPixels.data() + static_cast<size_t>(row) * decodeStride,
				expectedStride);
		}
	}

	return true;
#else
	LOGE("AImageDecoder is not supported on this Android API level");
	AAsset_close(asset);
	if (!decodeTextureWithJava(assetPath, outPixels, width, height)) {
		LOGE("Java fallback failed to decode %s", assetPath.c_str());
		return false;
	}
	return true;
#endif
}


