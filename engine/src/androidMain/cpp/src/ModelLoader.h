#pragma once

#include "Model.h"
#include "TeapotData.h"
#include "Model.h"
#include "TeapotData.h"

static inline Model loadModel() {
	Model model;
	model.positions.assign(
		TEAPOT_POSITIONS,
		TEAPOT_POSITIONS + TEAPOT_POSITION_COUNT
	);
	model.indices.assign(
		TEAPOT_INDICES,
		TEAPOT_INDICES + TEAPOT_INDEX_COUNT
	);
	return model;
}


