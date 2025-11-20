#include "ModelLoader.h"

#include <android/log.h>
#include <android/asset_manager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

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

std::string resolveAssetUri(const std::string& strBasePath, std::string strUri) {
	if (strUri.empty()) return {};
	if (strUri.compare(0, 5, "data:") == 0) {
		return {};
	}
	strUri = trim(strUri);
	if (strUri.empty()) return {};
	std::replace(strUri.begin(), strUri.end(), '\\', '/');
	while (strUri.compare(0, 2, "./") == 0) {
		strUri.erase(0, 2);
	}
	if (!strUri.empty() && (strUri.front() == '/' || strUri.front() == '\\')) {
		strUri.erase(strUri.begin());
	}
	if (!strBasePath.empty()) {
		const std::string strForwardPrefix = strBasePath + "/";
		const std::string strBackwardPrefix = strBasePath + "\\";
		const bool bHasForwardPrefix = strUri.rfind(strForwardPrefix, 0) == 0;
		const bool bHasBackwardPrefix = strUri.rfind(strBackwardPrefix, 0) == 0;
		if (!bHasForwardPrefix && !bHasBackwardPrefix) {
			strUri = joinPaths(strBasePath, strUri);
		}
	}
	std::replace(strUri.begin(), strUri.end(), '\\', '/');
	return strUri;
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

struct SAssetFileContext {
	AAssetManager* pAssetManager = nullptr;
	std::string strBasePath;
};

cgltf_result assetFileRead(const cgltf_memory_options* pMemoryOptions,
	const cgltf_file_options* pFileOptions,
	const char* path,
	cgltf_size* pSize,
	void** ppData) {
	if (!pFileOptions || !pSize || !ppData) {
		return cgltf_result_invalid_options;
	}
	const SAssetFileContext* pContext = static_cast<const SAssetFileContext*>(pFileOptions->user_data);
	if (!pContext || !pContext->pAssetManager || path == nullptr) {
		return cgltf_result_io_error;
	}
	std::string strAssetPath(path);
	const std::string strResolvedPath = resolveAssetUri(pContext->strBasePath, strAssetPath);
	const std::string strFileData = readAssetFile(pContext->pAssetManager, strResolvedPath);
	if (strFileData.empty()) {
		return cgltf_result_file_not_found;
	}
	const cgltf_size uBufferSize = static_cast<cgltf_size>(strFileData.size());
	void* pBuffer = nullptr;
	if (pMemoryOptions && pMemoryOptions->alloc_func) {
		pBuffer = pMemoryOptions->alloc_func(pMemoryOptions->user_data, uBufferSize);
	}
	if (!pBuffer) {
		pBuffer = std::malloc(uBufferSize);
		if (!pBuffer) {
			return cgltf_result_out_of_memory;
		}
	}
	std::memcpy(pBuffer, strFileData.data(), strFileData.size());
	*ppData = pBuffer;
	*pSize = uBufferSize;
	return cgltf_result_success;
}

void assetFileRelease(const cgltf_memory_options* pMemoryOptions,
	const cgltf_file_options*,
	void* pData,
	cgltf_size) {
	if (!pData) {
		return;
	}
	if (pMemoryOptions && pMemoryOptions->free_func) {
		pMemoryOptions->free_func(pMemoryOptions->user_data, pData);
	} else {
		std::free(pData);
	}
}

struct SCgltfDeleter {
	void operator()(cgltf_data* pData) const noexcept {
		if (pData) {
			cgltf_free(pData);
		}
	}
};

} // namespace

static Model loadObjModelInternal(AAssetManager* assetManager, const std::string& modelName) {
	Model model;
	if (!assetManager) {
		LOGE("loadObjModelInternal called with null asset manager");
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

	LOGI("Loaded OBJ model '%s': %zu vertices, %zu triangles, %zu materials",
		modelName.c_str(),
		model.vertexCount(),
		model.triangleCount(),
		model.materials.size());

	return model;
}

static Model loadGltfModelInternal(AAssetManager* pAssetManager, const std::string& strModelName) {
	Model sModel;
	if (!pAssetManager) {
		LOGE("loadGltfModelInternal called with null asset manager");
		return sModel;
	}
	if (strModelName.empty()) {
		LOGE("Model name is empty");
		return sModel;
	}

	const std::string strGltfText = readAssetFile(pAssetManager, strModelName);
	if (strGltfText.empty()) {
		LOGE("glTF asset is empty: %s", strModelName.c_str());
		return sModel;
	}

	const std::string strBaseDir = directoryOf(strModelName);

	SAssetFileContext sFileContext;
	sFileContext.pAssetManager = pAssetManager;
	sFileContext.strBasePath = strBaseDir;

	cgltf_options sOptions{};
	sOptions.type = cgltf_file_type_gltf;
	sOptions.file.read = assetFileRead;
	sOptions.file.release = assetFileRelease;
	sOptions.file.user_data = &sFileContext;

	cgltf_data* pRawData = nullptr;
	cgltf_result eParseResult = cgltf_parse(&sOptions, strGltfText.data(), strGltfText.size(), &pRawData);
	if (eParseResult != cgltf_result_success) {
		LOGE("Failed to parse glTF file %s (error %d)", strModelName.c_str(), static_cast<int>(eParseResult));
		return sModel;
	}
	std::unique_ptr<cgltf_data, SCgltfDeleter> pData(pRawData);

	const char* pszBasePath = strBaseDir.empty() ? nullptr : strBaseDir.c_str();
	cgltf_result eBufferResult = cgltf_load_buffers(&sOptions, pData.get(), pszBasePath);
	if (eBufferResult != cgltf_result_success) {
		LOGE("Failed to load glTF buffers for %s (error %d)", strModelName.c_str(), static_cast<int>(eBufferResult));
		return sModel;
	}

	std::unordered_map<const cgltf_material*, uint16_t> mapMaterialByPointer;
	std::unordered_map<std::string, uint16_t> mapMaterialByName;

	if (pData->materials_count > 0) {
		mapMaterialByPointer.reserve(pData->materials_count);
		mapMaterialByName.reserve(pData->materials_count);
		for (cgltf_size uMaterialIndex = 0; uMaterialIndex < pData->materials_count; ++uMaterialIndex) {
			const cgltf_material& sSourceMaterial = pData->materials[uMaterialIndex];
			Material sMappedMaterial;
			if (sSourceMaterial.name && sSourceMaterial.name[0] != '\0') {
				sMappedMaterial.name = sSourceMaterial.name;
			} else {
				sMappedMaterial.name = "Material_" + std::to_string(uMaterialIndex);
			}
			if (sSourceMaterial.has_pbr_metallic_roughness) {
				const cgltf_pbr_metallic_roughness& sPbr = sSourceMaterial.pbr_metallic_roughness;
				sMappedMaterial.diffuseColor[0] = sPbr.base_color_factor[0];
				sMappedMaterial.diffuseColor[1] = sPbr.base_color_factor[1];
				sMappedMaterial.diffuseColor[2] = sPbr.base_color_factor[2];
				const cgltf_texture_view& sBaseColorTexture = sPbr.base_color_texture;
				if (sBaseColorTexture.texture && sBaseColorTexture.texture->image) {
					const cgltf_image* pImage = sBaseColorTexture.texture->image;
					if (pImage->uri && pImage->uri[0] != '\0') {
						const std::string strTexturePath = resolveAssetUri(strBaseDir, pImage->uri);
						if (!strTexturePath.empty()) {
							sMappedMaterial.diffuseTexture = strTexturePath;
						}
					}
				}
			}
			const uint16_t uMaterialSlot = static_cast<uint16_t>(sModel.materials.size());
			sModel.materials.push_back(sMappedMaterial);
			mapMaterialByPointer[&sSourceMaterial] = uMaterialSlot;
			mapMaterialByName[sMappedMaterial.name] = uMaterialSlot;
		}
	}

	if (sModel.materials.empty()) {
		Material sDefaultMaterial;
		sDefaultMaterial.name = "Default";
		sModel.materials.push_back(sDefaultMaterial);
		mapMaterialByName["Default"] = 0;
	}

	sModel.subsets.clear();

	for (cgltf_size uMeshIndex = 0; uMeshIndex < pData->meshes_count; ++uMeshIndex) {
		const cgltf_mesh& sMesh = pData->meshes[uMeshIndex];
		for (cgltf_size uPrimitiveIndex = 0; uPrimitiveIndex < sMesh.primitives_count; ++uPrimitiveIndex) {
			const cgltf_primitive& sPrimitive = sMesh.primitives[uPrimitiveIndex];
			if (sPrimitive.type != cgltf_primitive_type_triangles) {
				LOGE("Unsupported primitive type %d in glTF model %s", static_cast<int>(sPrimitive.type), strModelName.c_str());
				continue;
			}

			const cgltf_accessor* pPositionAccessor = nullptr;
			const cgltf_accessor* pNormalAccessor = nullptr;
			const cgltf_accessor* pTexcoordAccessor = nullptr;

			for (cgltf_size uAttributeIndex = 0; uAttributeIndex < sPrimitive.attributes_count; ++uAttributeIndex) {
				const cgltf_attribute& sAttribute = sPrimitive.attributes[uAttributeIndex];
				if (sAttribute.type == cgltf_attribute_type_position) {
					pPositionAccessor = sAttribute.data;
				} else if (sAttribute.type == cgltf_attribute_type_normal) {
					pNormalAccessor = sAttribute.data;
				} else if (sAttribute.type == cgltf_attribute_type_texcoord && sAttribute.index == 0) {
					pTexcoordAccessor = sAttribute.data;
				}
			}

			if (!pPositionAccessor || pPositionAccessor->count == 0) {
				LOGE("glTF primitive missing positions in %s", strModelName.c_str());
				continue;
			}

			const cgltf_size uVertexCount = pPositionAccessor->count;
			const uint32_t uVertexBase = static_cast<uint32_t>(sModel.vertexCount());

			sModel.positions.reserve(sModel.positions.size() + static_cast<size_t>(uVertexCount) * 3);
			sModel.normals.reserve(sModel.normals.size() + static_cast<size_t>(uVertexCount) * 3);
			sModel.texcoords.reserve(sModel.texcoords.size() + static_cast<size_t>(uVertexCount) * 2);

			for (cgltf_size uVertexIndex = 0; uVertexIndex < uVertexCount; ++uVertexIndex) {
				float afPosition[3] = {0.0f, 0.0f, 0.0f};
				cgltf_accessor_read_float(pPositionAccessor, uVertexIndex, afPosition, 3);
				sModel.positions.push_back(afPosition[0]);
				sModel.positions.push_back(afPosition[1]);
				sModel.positions.push_back(afPosition[2]);

				if (pNormalAccessor) {
					float afNormal[3] = {0.0f, 0.0f, 0.0f};
					if (cgltf_accessor_read_float(pNormalAccessor, uVertexIndex, afNormal, 3)) {
						sModel.normals.push_back(afNormal[0]);
						sModel.normals.push_back(afNormal[1]);
						sModel.normals.push_back(afNormal[2]);
					} else {
						sModel.normals.insert(sModel.normals.end(), {0.0f, 0.0f, 0.0f});
					}
				} else {
					sModel.normals.insert(sModel.normals.end(), {0.0f, 0.0f, 0.0f});
				}

				if (pTexcoordAccessor) {
					float afTexcoord[2] = {0.0f, 0.0f};
					if (cgltf_accessor_read_float(pTexcoordAccessor, uVertexIndex, afTexcoord, 2)) {
						sModel.texcoords.push_back(afTexcoord[0]);
						sModel.texcoords.push_back(afTexcoord[1]);
					} else {
						sModel.texcoords.insert(sModel.texcoords.end(), {0.0f, 0.0f});
					}
				} else {
					sModel.texcoords.insert(sModel.texcoords.end(), {0.0f, 0.0f});
				}
			}

			const cgltf_accessor* pIndicesAccessor = sPrimitive.indices;
			const cgltf_size uIndexCount = pIndicesAccessor ? pIndicesAccessor->count : uVertexCount;
			if (uIndexCount == 0) {
				continue;
			}

			sModel.indices.reserve(sModel.indices.size() + static_cast<size_t>(uIndexCount));

			const uint32_t uIndexOffset = static_cast<uint32_t>(sModel.indices.size());
			if (pIndicesAccessor) {
				for (cgltf_size uIndex = 0; uIndex < uIndexCount; ++uIndex) {
					const cgltf_size uValue = cgltf_accessor_read_index(pIndicesAccessor, uIndex);
					sModel.indices.push_back(uVertexBase + static_cast<uint32_t>(uValue));
				}
			} else {
				for (cgltf_size uIndex = 0; uIndex < uVertexCount; ++uIndex) {
					sModel.indices.push_back(uVertexBase + static_cast<uint32_t>(uIndex));
				}
			}

			uint16_t uMaterialIndex = 0;
			if (sPrimitive.material) {
				auto itMaterial = mapMaterialByPointer.find(sPrimitive.material);
				if (itMaterial != mapMaterialByPointer.end()) {
					uMaterialIndex = itMaterial->second;
				} else {
					std::string strGeneratedName;
					if (sPrimitive.material->name && sPrimitive.material->name[0] != '\0') {
						strGeneratedName = sPrimitive.material->name;
					} else {
						strGeneratedName = "Material";
					}
					uMaterialIndex = ensureMaterial(sModel, strGeneratedName, mapMaterialByName);
				}
			}

			Model::Subset sSubset;
			sSubset.indexOffset = uIndexOffset;
			sSubset.indexCount = static_cast<uint32_t>(sModel.indices.size()) - uIndexOffset;
			sSubset.materialIndex = uMaterialIndex;

			if (!sModel.subsets.empty()) {
				Model::Subset& sLastSubset = sModel.subsets.back();
				if (sLastSubset.materialIndex == sSubset.materialIndex &&
					sLastSubset.indexOffset + sLastSubset.indexCount == sSubset.indexOffset) {
					sLastSubset.indexCount += sSubset.indexCount;
					continue;
				}
			}

			sModel.subsets.push_back(sSubset);
		}
	}

	LOGI("Loaded glTF model '%s': %zu vertices, %zu triangles, %zu materials",
		strModelName.c_str(),
		sModel.vertexCount(),
		sModel.triangleCount(),
		sModel.materials.size());

	return sModel;
}

Model loadModel(AAssetManager* pAssetManager, const std::string& strModelName) {
	if (!pAssetManager) {
		LOGE("loadModel called with null asset manager");
		return {};
	}
	if (strModelName.empty()) {
		LOGE("Model name is empty");
		return {};
	}

	std::string strNormalizedName = strModelName;
	std::transform(strNormalizedName.begin(), strNormalizedName.end(), strNormalizedName.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});

	std::string strExtension;
	const std::string::size_type uDotPos = strNormalizedName.find_last_of('.');
	if (uDotPos != std::string::npos) {
		strExtension = strNormalizedName.substr(uDotPos);
	}

	if (strExtension == ".obj") {
		return loadObjModelInternal(pAssetManager, strModelName);
	}
	if (strExtension == ".gltf") {
		return loadGltfModelInternal(pAssetManager, strModelName);
	}

	LOGE("Unsupported model format: %s", strModelName.c_str());
	return {};
}


