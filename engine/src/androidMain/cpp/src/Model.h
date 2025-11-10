#pragma once

#include <vector>
#include <cstdint>

// Simple 3D model container for positions and indices.
struct Model {
	std::vector<float> positions;   // xyz sequence
	std::vector<uint16_t> indices;  // triangle indices
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


