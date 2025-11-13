#pragma once

#include <android/asset_manager.h>
#include <string>
#include <vector>

bool LoadImageFromAssets(AAssetManager* assetManager,
	const std::string& assetPath,
	int desiredChannels,
	std::vector<unsigned char>& outPixels,
	int& width,
	int& height);


