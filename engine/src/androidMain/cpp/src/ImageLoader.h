#pragma once

#include <string>
#include <vector>

bool LoadImageFromFile(const std::string& strPath,
	int nDesiredChannels,
	std::vector<unsigned char>& outPixels,
	int& nWidth,
	int& nHeight);


