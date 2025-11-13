#pragma once

#include <vector>
#include <cstdint>

// Simple 3D model container for geometry data and transform.
struct Model {
	std::vector<float> positions;   // xyz sequence
	std::vector<uint16_t> indices;  // triangle indices
	float position[3] = { 0.0f, 0.0f, 0.0f }; // model translation

	void setPosition(float x, float y, float z) {
		position[0] = x;
		position[1] = y;
		position[2] = z;
	}

	size_t vertexCount() const {
		return positions.size() / 3;
	}

	size_t indexCount() const {
		return indices.size();
	}

	bool hasGeometry() const {
		return !positions.empty() && !indices.empty();
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

	const uint16_t idx[] = {
		0, 1, 2,  2, 3, 0,       // back
		4, 6, 5,  6, 4, 7,       // front
		0, 3, 7,  7, 4, 0,       // left
		1, 5, 6,  6, 2, 1,       // right
		3, 2, 6,  6, 7, 3,       // top
		0, 4, 5,  5, 1, 0        // bottom
	};
	m.indices.assign(idx, idx + 36);
	return m;
}


