#pragma once

#ifdef __APPLE__
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* EngineIosHandle;

EngineIosHandle engine_ios_create();
void engine_ios_destroy(EngineIosHandle handle);
bool engine_ios_initialize(EngineIosHandle handle, uint32_t width, uint32_t height);
void engine_ios_resize(EngineIosHandle handle, uint32_t width, uint32_t height);
void engine_ios_render(EngineIosHandle handle);
void engine_ios_shutdown(EngineIosHandle handle);
void engine_ios_load_model(EngineIosHandle handle, const char* path, int64_t modelId, float x, float y, float z, float scale);
void engine_ios_translate_model(EngineIosHandle handle, int64_t modelId, float x, float y, float z);
void engine_ios_scale_model(EngineIosHandle handle, int64_t modelId, float scale);
void engine_ios_rotate_model(EngineIosHandle handle, int64_t modelId, float x, float y, float z);
void engine_ios_move_camera(EngineIosHandle handle, float delta);
void engine_ios_set_camera_position(EngineIosHandle handle, float x, float y, float z);
void engine_ios_set_camera_rotation(EngineIosHandle handle, float yaw, float pitch, float roll);
void engine_ios_rotate_camera(EngineIosHandle handle, float yaw, float pitch, float roll);

#ifdef __cplusplus
}
#endif
#endif


