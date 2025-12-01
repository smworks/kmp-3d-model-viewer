#include "ImageLoader.h"

#include <android/log.h>
#include <jni.h>

#include <cstring>
#include <string>
#include <vector>

#define LOG_TAG "ImageLoader"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern JavaVM* g_javaVm;
extern jclass g_engineApiClass;
extern jmethodID g_loadFileMethod;

namespace {

JNIEnv* getEnv(bool& bDidAttach) {
	bDidAttach = false;
	if (!g_javaVm) return nullptr;
	JNIEnv* pEnv = nullptr;
	jint nStatus = g_javaVm->GetEnv(reinterpret_cast<void**>(&pEnv), JNI_VERSION_1_6);
	if (nStatus == JNI_EDETACHED) {
		if (g_javaVm->AttachCurrentThread(&pEnv, nullptr) != JNI_OK) {
			return nullptr;
		}
		bDidAttach = true;
	} else if (nStatus != JNI_OK) {
		return nullptr;
	}
	return pEnv;
}

void releaseEnv(bool bDidAttach) {
	if (bDidAttach && g_javaVm) {
		g_javaVm->DetachCurrentThread();
	}
}

bool loadFileWithJava(const std::string& strPath, std::vector<unsigned char>& vecBytes) {
	if (!g_engineApiClass || !g_loadFileMethod) {
		return false;
	}
	bool bDidAttach = false;
	JNIEnv* pEnv = getEnv(bDidAttach);
	if (!pEnv) {
		releaseEnv(bDidAttach);
		return false;
	}

	jstring jPath = pEnv->NewStringUTF(strPath.c_str());
	if (!jPath) {
		releaseEnv(bDidAttach);
		return false;
	}

	jbyteArray jResult = static_cast<jbyteArray>(pEnv->CallStaticObjectMethod(g_engineApiClass, g_loadFileMethod, jPath));
	pEnv->DeleteLocalRef(jPath);

	if (pEnv->ExceptionCheck()) {
		pEnv->ExceptionClear();
		if (jResult) {
			pEnv->DeleteLocalRef(jResult);
		}
		releaseEnv(bDidAttach);
		return false;
	}

	if (!jResult) {
		releaseEnv(bDidAttach);
		return false;
	}

	jsize nLength = pEnv->GetArrayLength(jResult);
	if (nLength <= 0) {
		pEnv->DeleteLocalRef(jResult);
		releaseEnv(bDidAttach);
		return false;
	}

	vecBytes.resize(static_cast<size_t>(nLength));
	jbyte* pData = pEnv->GetByteArrayElements(jResult, nullptr);
	if (!pData) {
		pEnv->DeleteLocalRef(jResult);
		vecBytes.clear();
		releaseEnv(bDidAttach);
		return false;
	}

	std::memcpy(vecBytes.data(), pData, static_cast<size_t>(nLength));
	pEnv->ReleaseByteArrayElements(jResult, pData, JNI_ABORT);
	pEnv->DeleteLocalRef(jResult);
	releaseEnv(bDidAttach);

	return !vecBytes.empty();
}

} // namespace

bool LoadImageFromFile(const std::string& strPath,
	int nDesiredChannels,
	std::vector<unsigned char>& outPixels,
	int& nWidth,
	int& nHeight) {
	outPixels.clear();
	nWidth = 0;
	nHeight = 0;

	std::vector<unsigned char> vecEncoded;
	if (!loadFileWithJava(strPath, vecEncoded)) {
		LOGE("Failed to load file bytes for %s", strPath.c_str());
		return false;
	}
	if (vecEncoded.empty()) {
		LOGE("File %s returned empty byte array", strPath.c_str());
		return false;
	}

	int nChannelsInFile = 0;
	unsigned char* pDecoded = stbi_load_from_memory(
		vecEncoded.data(),
		static_cast<int>(vecEncoded.size()),
		&nWidth,
		&nHeight,
		&nChannelsInFile,
		nDesiredChannels);
	if (!pDecoded) {
		const char* pcReason = stbi_failure_reason();
		LOGE("stbi_load_from_memory failed for %s (%s)", strPath.c_str(), pcReason ? pcReason : "unknown");
		return false;
	}

	const int nOutputChannels = nDesiredChannels > 0 ? nDesiredChannels : nChannelsInFile;
	if (nWidth <= 0 || nHeight <= 0 || nOutputChannels <= 0) {
		stbi_image_free(pDecoded);
		return false;
	}

	const size_t nExpectedSize = static_cast<size_t>(nWidth) *
		static_cast<size_t>(nHeight) *
		static_cast<size_t>(nOutputChannels);
	outPixels.assign(pDecoded, pDecoded + nExpectedSize);
	stbi_image_free(pDecoded);

	return !outPixels.empty();
}
