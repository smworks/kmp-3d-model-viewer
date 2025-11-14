#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <array>

struct Material {
	std::string name;
	std::array<float, 3> diffuseColor{ 1.0f, 1.0f, 1.0f };
	std::string diffuseTexture; // Relative path inside assets folder
};

// Simple 3D model container for geometry, materials and transform.
struct Model {
	std::vector<float> positions;        // xyz sequence
	std::vector<float> normals;          // xyz sequence
	std::vector<float> texcoords;        // uv sequence
	std::vector<uint32_t> indices;       // triangle indices
	std::vector<Material> materials;
	struct Subset {
		uint32_t indexOffset = 0;   // Index into indices vector
		uint32_t indexCount = 0;    // Count of indices for this subset
		uint16_t materialIndex = 0; // Index into materials vector
	};
	std::vector<Subset> subsets;
	float position[3] = { 0.0f, 0.0f, 0.0f }; // model translation
	float scale = 1.0f;
	float rotation[3] = { 0.0f, 0.0f, 0.0f };

	void setPosition(float x, float y, float z) {
		position[0] = x;
		position[1] = y;
		position[2] = z;
	}

	void setScale(float value) {
		scale = value;
	}

	void setRotation(float x, float y, float z) {
		rotation[0] = x;
		rotation[1] = y;
		rotation[2] = z;
	}

	size_t vertexCount() const {
		return positions.size() / 3;
	}

	size_t indexCount() const {
		return indices.size();
	}

	size_t triangleCount() const {
		return indices.size() / 3;
	}

	bool hasGeometry() const {
		return !positions.empty() && !indices.empty();
	}

	bool hasNormals() const {
		return !normals.empty();
	}

	bool hasTexcoords() const {
		return !texcoords.empty();
	}
};

// Creates a unit cube centered at origin.
static inline Model createCubeModel() {
	Model m;
	const float p = 0.5f;
	const float verts[] = {
		-p, -p, -p,  p, -p, -p,  p,  p, -p,  -p,  p, -p, // back
		-p, -p,  p,  p, -p,  p,  p,  p,  p,  -p,  p,  p  // front
	};
	m.positions.assign(verts, verts + 8 * 3);

	const uint32_t idx[] = {
		0, 1, 2,  2, 3, 0,       // back
		4, 6, 5,  6, 4, 7,       // front
		0, 3, 7,  7, 4, 0,       // left
		1, 5, 6,  6, 2, 1,       // right
		3, 2, 6,  6, 7, 3,       // top
		0, 4, 5,  5, 1, 0        // bottom
	};
	m.indices.assign(idx, idx + 36);
	Material mat;
	mat.name = "Default";
	m.materials.push_back(mat);
	Model::Subset subset;
	subset.indexOffset = 0;
	subset.indexCount = static_cast<uint32_t>(m.indices.size());
	subset.materialIndex = 0;
	m.subsets.push_back(subset);
	return m;
}


