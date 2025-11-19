#include "ModelLoader.h"

#include <android/log.h>
#include <android/asset_manager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

#define LOG_TAG "ModelLoader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr int kMissingIndex = std::numeric_limits<int>::min();

struct VertexKey {
	int position = kMissingIndex;
	int texcoord = kMissingIndex;
	int normal = kMissingIndex;

	bool operator==(const VertexKey& other) const noexcept {
		return position == other.position &&
			texcoord == other.texcoord &&
			normal == other.normal;
	}
};

struct VertexKeyHash {
	size_t operator()(const VertexKey& key) const noexcept {
		size_t h = static_cast<size_t>(std::hash<int>()(key.position));
		h = h * 31u + static_cast<size_t>(std::hash<int>()(key.texcoord));
		h = h * 31u + static_cast<size_t>(std::hash<int>()(key.normal));
		return h;
	}
};

std::string trim(const std::string& value) {
	auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
	auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
	if (begin >= end) {
		return std::string();
	}
	return std::string(begin, end);
}

std::string readAssetFile(AAssetManager* manager, const std::string& path) {
	if (!manager) return {};
	AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_STREAMING);
	if (!asset) {
		LOGE("Failed to open asset: %s", path.c_str());
		return {};
	}
	off_t length = AAsset_getLength(asset);
	if (length <= 0) {
		AAsset_close(asset);
		return {};
	}
	std::string contents(static_cast<size_t>(length), '\0');
	int64_t read = AAsset_read(asset, contents.data(), length);
	AAsset_close(asset);
	if (read < 0) {
		LOGE("Failed to read asset: %s", path.c_str());
		return {};
	}
	if (read != length) {
		contents.resize(static_cast<size_t>(read));
	}
	return contents;
}

std::string directoryOf(const std::string& path) {
	const auto pos = path.find_last_of("/\\");
	if (pos == std::string::npos) return {};
	return path.substr(0, pos);
}

std::string joinPaths(const std::string& dir, const std::string& file) {
	if (dir.empty()) return file;
	if (file.empty()) return dir;
	if (!file.empty() && (file.front() == '/' || file.front() == '\\')) {
		return file;
	}
	return dir + "/" + file;
}

bool parseInt(const std::string& text, int& value) {
	if (text.empty()) return false;
	char* end = nullptr;
	const long parsed = std::strtol(text.c_str(), &end, 10);
	if (end == text.c_str()) return false;
	value = static_cast<int>(parsed);
	return true;
}

bool parseVertexToken(const std::string& token, int& v, int& vt, int& vn) {
	if (token.empty()) return false;
	std::string first, second, third;
	size_t start = 0;
	size_t slash = token.find('/', start);
	first = slash == std::string::npos ? token : token.substr(start, slash - start);
	if (slash != std::string::npos) {
		start = slash + 1;
		slash = token.find('/', start);
		second = slash == std::string::npos ? token.substr(start) : token.substr(start, slash - start);
		if (slash != std::string::npos) {
			start = slash + 1;
			if (start <= token.size()) {
				third = token.substr(start);
			}
		}
	}

	if (!parseInt(first, v)) return false;
	vt = kMissingIndex;
	vn = kMissingIndex;
	if (!second.empty()) {
		int parsed = 0;
		if (parseInt(second, parsed)) {
			vt = parsed;
		}
	}
	if (!third.empty()) {
		int parsed = 0;
		if (parseInt(third, parsed)) {
			vn = parsed;
		}
	}
	return true;
}

int resolveIndex(int index, size_t count) {
	if (index == kMissingIndex) return kMissingIndex;
	if (index > 0) {
		return index - 1;
	}
	if (index < 0) {
		int resolved = static_cast<int>(count) + index;
		return resolved;
	}
	return kMissingIndex;
}

uint16_t ensureMaterial(Model& model, const std::string& name, std::unordered_map<std::string, uint16_t>& lookup) {
	auto it = lookup.find(name);
	if (it != lookup.end()) {
		return it->second;
	}
	Material material;
	material.name = name;
	model.materials.push_back(material);
	const uint16_t index = static_cast<uint16_t>(model.materials.size() - 1);
	lookup[name] = index;
	return index;
}

std::string resolveMapPath(const std::string& baseDir, const std::string& spec) {
	std::string trimmed = trim(spec);
	if (trimmed.empty()) return {};

	std::vector<std::string> tokens;
	tokens.reserve(8);

	std::string current;
	bool inQuotes = false;
	char quoteChar = '\0';

	auto flushToken = [&]() {
		if (!current.empty()) {
			tokens.push_back(current);
			current.clear();
		}
	};

	for (char ch : trimmed) {
		if (inQuotes) {
			if (ch == quoteChar) {
				inQuotes = false;
				flushToken();
			} else {
				current.push_back(ch);
			}
		} else {
			if (ch == '"' || ch == '\'') {
				flushToken();
				inQuotes = true;
				quoteChar = ch;
			} else if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
				flushToken();
			} else {
				current.push_back(ch);
			}
		}
	}
	flushToken();

	auto isLikelyPath = [](const std::string& value) {
		return value.find('/') != std::string::npos ||
			value.find('\\') != std::string::npos ||
			value.find('.') != std::string::npos;
	};

	std::string candidate;
	for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
		const std::string& value = *it;
		if (value.empty()) {
			continue;
		}
		if (value[0] == '-') {
			continue;
		}
		if (!candidate.empty() && !isLikelyPath(value)) {
			continue;
		}
		if (isLikelyPath(value) || candidate.empty()) {
			candidate = value;
			if (isLikelyPath(value)) {
				break;
			}
		}
	}

	if (candidate.empty() && !tokens.empty()) {
		candidate = tokens.back();
	}

	candidate = trim(candidate);
	if (candidate.empty()) return {};

	std::replace(candidate.begin(), candidate.end(), '\\', '/');
	while (candidate.compare(0, 2, "./") == 0) {
		candidate.erase(0, 2);
	}
	if (!candidate.empty() && candidate.front() == '/') {
		candidate.erase(candidate.begin());
	}

	if (!baseDir.empty()) {
		const std::string prefix = baseDir + "/";
		if (candidate.rfind(prefix, 0) == 0) {
			return candidate;
		}
	}

	return joinPaths(baseDir, candidate);
}

void parseMtlContents(const std::string& mtlText, const std::string& baseDir, Model& model, std::unordered_map<std::string, uint16_t>& materialLookup) {
	if (mtlText.empty()) return;

	std::istringstream stream(mtlText);
	std::string line;
	Material currentMaterial;
	bool hasMaterial = false;

	auto pushMaterial = [&]() {
		if (!hasMaterial || currentMaterial.name.empty()) return;
		auto it = materialLookup.find(currentMaterial.name);
		if (it != materialLookup.end()) {
			model.materials[it->second] = currentMaterial;
		} else {
			model.materials.push_back(currentMaterial);
			uint16_t idx = static_cast<uint16_t>(model.materials.size() - 1);
			materialLookup[currentMaterial.name] = idx;
		}
		hasMaterial = false;
	};

	while (std::getline(stream, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::istringstream lineStream(line);
		std::string keyword;
		lineStream >> keyword;
		std::string remainder;
		std::getline(lineStream, remainder);
		remainder = trim(remainder);

		if (keyword == "newmtl") {
			pushMaterial();
			currentMaterial = Material{};
			currentMaterial.name = remainder;
			hasMaterial = true;
		} else if (keyword == "Kd" && hasMaterial) {
			std::istringstream kdStream(remainder);
			kdStream >> currentMaterial.diffuseColor[0] >> currentMaterial.diffuseColor[1] >> currentMaterial.diffuseColor[2];
		} else if ((keyword == "map_Kd" || keyword == "map_Ka") && hasMaterial) {
			const std::string texturePath = resolveMapPath(baseDir, remainder);
			if (!texturePath.empty()) {
				currentMaterial.diffuseTexture = texturePath;
			}
		}
	}

	pushMaterial();
}

} // namespace

Model loadModel(AAssetManager* assetManager, const std::string& modelName) {
	Model model;
	if (!assetManager) {
		LOGE("loadModel called with null asset manager");
		return model;
	}
	if (modelName.empty()) {
		LOGE("Model name is empty");
		return model;
	}

	const std::string objText = readAssetFile(assetManager, modelName);
	if (objText.empty()) {
		LOGE("OBJ asset is empty: %s", modelName.c_str());
		return model;
	}

	const std::string baseDir = directoryOf(modelName);

	model.materials.clear();
	Material defaultMaterial;
	defaultMaterial.name = "Default";
	model.materials.push_back(defaultMaterial);
	model.subsets.clear();

	std::unordered_map<std::string, uint16_t> materialLookup;
	materialLookup["Default"] = 0;

	std::vector<std::array<float, 3>> positionsRaw;
	std::vector<std::array<float, 3>> normalsRaw;
	std::vector<std::array<float, 2>> texcoordsRaw;

	std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexLookup;
	int currentMaterialIndex = 0;

	std::istringstream stream(objText);
	std::string line;
	while (std::getline(stream, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#') continue;

		std::istringstream lineStream(line);
		std::string keyword;
		lineStream >> keyword;
		std::string remainder;
		std::getline(lineStream, remainder);
		remainder = trim(remainder);

		if (keyword == "mtllib") {
			std::istringstream libs(remainder);
			std::string mtlFile;
			while (libs >> mtlFile) {
				const std::string mtlPath = joinPaths(baseDir, mtlFile);
				const std::string mtlText = readAssetFile(assetManager, mtlPath);
				if (mtlText.empty()) {
					LOGE("Failed to read MTL file: %s", mtlPath.c_str());
					continue;
				}
				parseMtlContents(mtlText, baseDir, model, materialLookup);
			}
		} else if (keyword == "usemtl") {
			if (!remainder.empty()) {
				currentMaterialIndex = ensureMaterial(model, remainder, materialLookup);
			}
		} else if (keyword == "v") {
			std::istringstream values(remainder);
			std::array<float, 3> pos{0.0f, 0.0f, 0.0f};
			values >> pos[0] >> pos[1] >> pos[2];
			positionsRaw.push_back(pos);
		} else if (keyword == "vn") {
			std::istringstream values(remainder);
			std::array<float, 3> normal{0.0f, 0.0f, 0.0f};
			values >> normal[0] >> normal[1] >> normal[2];
			normalsRaw.push_back(normal);
		} else if (keyword == "vt") {
			std::istringstream values(remainder);
			std::array<float, 2> tex{0.0f, 0.0f};
			values >> tex[0] >> tex[1];
			texcoordsRaw.push_back(tex);
		} else if (keyword == "f") {
			std::istringstream faceStream(remainder);
			std::vector<uint32_t> faceIndices;
			std::string vertexToken;
			while (faceStream >> vertexToken) {
				int vIdx = 0;
				int vtIdx = kMissingIndex;
				int vnIdx = kMissingIndex;
				if (!parseVertexToken(vertexToken, vIdx, vtIdx, vnIdx)) {
					LOGE("Failed to parse vertex token: %s", vertexToken.c_str());
					continue;
				}

				const int resolvedV = resolveIndex(vIdx, positionsRaw.size());
				const int resolvedVt = resolveIndex(vtIdx, texcoordsRaw.size());
				const int resolvedVn = resolveIndex(vnIdx, normalsRaw.size());

				if (resolvedV < 0 || resolvedV >= static_cast<int>(positionsRaw.size())) {
					LOGE("Invalid position index %d in model %s", resolvedV, modelName.c_str());
					continue;
				}
				if (resolvedVt != kMissingIndex && (resolvedVt < 0 || resolvedVt >= static_cast<int>(texcoordsRaw.size()))) {
					LOGE("Invalid texcoord index %d in model %s", resolvedVt, modelName.c_str());
					continue;
				}
				if (resolvedVn != kMissingIndex && (resolvedVn < 0 || resolvedVn >= static_cast<int>(normalsRaw.size()))) {
					LOGE("Invalid normal index %d in model %s", resolvedVn, modelName.c_str());
					continue;
				}

				const VertexKey key{ resolvedV, resolvedVt, resolvedVn };
				auto it = vertexLookup.find(key);
				if (it == vertexLookup.end()) {
					const size_t currentVertexCount = model.vertexCount();
					const auto& pos = positionsRaw[resolvedV];
					model.positions.push_back(pos[0]);
					model.positions.push_back(pos[1]);
					model.positions.push_back(pos[2]);

					if (resolvedVn != kMissingIndex) {
						const auto& normal = normalsRaw[resolvedVn];
						model.normals.push_back(normal[0]);
						model.normals.push_back(normal[1]);
						model.normals.push_back(normal[2]);
					} else {
						model.normals.insert(model.normals.end(), {0.0f, 0.0f, 0.0f});
					}

					if (resolvedVt != kMissingIndex) {
						const auto& tex = texcoordsRaw[resolvedVt];
						model.texcoords.push_back(tex[0]);
						model.texcoords.push_back(1.0f - tex[1]);
					} else {
						model.texcoords.insert(model.texcoords.end(), {0.0f, 0.0f});
					}

					const uint32_t newIndex = static_cast<uint32_t>(currentVertexCount);
					vertexLookup[key] = newIndex;
					faceIndices.push_back(newIndex);
				} else {
					faceIndices.push_back(it->second);
				}
			}

			if (faceIndices.size() < 3) continue;

			for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
				const uint32_t baseIndex = static_cast<uint32_t>(model.indices.size());
				model.indices.push_back(faceIndices[0]);
				model.indices.push_back(faceIndices[i]);
				model.indices.push_back(faceIndices[i + 1]);
				const uint16_t matIndex = static_cast<uint16_t>(
					(currentMaterialIndex >= 0 && currentMaterialIndex < static_cast<int>(model.materials.size()))
						? currentMaterialIndex
						: 0);
				if (!model.subsets.empty()) {
					Model::Subset& last = model.subsets.back();
					if (last.materialIndex == matIndex && last.indexOffset + last.indexCount == baseIndex) {
						last.indexCount += 3;
						continue;
					}
				}
				Model::Subset subset;
				subset.indexOffset = baseIndex;
				subset.indexCount = 3;
				subset.materialIndex = matIndex;
				model.subsets.push_back(subset);
			}
		}
	}

	LOGI("Loaded model '%s': %zu vertices, %zu triangles, %zu materials",
		modelName.c_str(),
		model.vertexCount(),
		model.triangleCount(),
		model.materials.size());

	return model;
}


