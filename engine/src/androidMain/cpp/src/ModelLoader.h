#pragma once

#include <string>
#include <android/asset_manager.h>

#include "Model.h"

Model loadModel(AAssetManager* assetManager, const std::string& modelName);


