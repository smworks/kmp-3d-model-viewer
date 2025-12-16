#include "EngineIosBridge.h"

#ifdef __APPLE__
#include "EngineCore.h"
#include "GraphicsLayer.h"
#include <memory>

namespace
{
	using engine::CEngineCore;
	using engine::CIosGraphicsLayer;
	using engine::GraphicsLayerPtr;
	using engine::SFrameConfig;
	using engine::SInitConfig;
	using engine::SModelDescription;

	struct EngineIosContext
	{
		std::unique_ptr<CEngineCore> pCore;
	};

	GraphicsLayerPtr createIosLayer()
	{
		return std::make_unique<CIosGraphicsLayer>();
	}

	EngineIosContext* toContext(EngineIosHandle handle)
	{
		return reinterpret_cast<EngineIosContext*>(handle);
	}
}

EngineIosHandle engine_ios_create()
{
	auto* pContext = new EngineIosContext();
	pContext->pCore = std::make_unique<CEngineCore>(createIosLayer());
	return reinterpret_cast<EngineIosHandle>(pContext);
}

void engine_ios_destroy(EngineIosHandle handle)
{
	auto* pContext = toContext(handle);
	if (!pContext)
	{
		return;
	}
	pContext->pCore.reset();
	delete pContext;
}

bool engine_ios_initialize(EngineIosHandle handle, uint32_t width, uint32_t height)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return false;
	}
	SInitConfig config;
	config.uWidth = width;
	config.uHeight = height;
	return pContext->pCore->bInitialize(config);
}

void engine_ios_resize(EngineIosHandle handle, uint32_t width, uint32_t height)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	SFrameConfig frame;
	frame.uWidth = width;
	frame.uHeight = height;
	pContext->pCore->Resize(frame);
}

void engine_ios_render(EngineIosHandle handle)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->Render();
}

void engine_ios_shutdown(EngineIosHandle handle)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->Shutdown();
}

void engine_ios_load_model(EngineIosHandle handle, const char* path, int64_t modelId, float x, float y, float z, float scale)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore || !path)
	{
		return;
	}
	SModelDescription desc;
	desc.llId = modelId;
	desc.sAssetPath = path;
	desc.sTransform.fPosition[0] = x;
	desc.sTransform.fPosition[1] = y;
	desc.sTransform.fPosition[2] = z;
	desc.sTransform.fScale = scale;
	pContext->pCore->LoadModel(desc);
}

void engine_ios_translate_model(EngineIosHandle handle, int64_t modelId, float x, float y, float z)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->TranslateModel(modelId, x, y, z);
}

void engine_ios_scale_model(EngineIosHandle handle, int64_t modelId, float scale)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->ScaleModel(modelId, scale);
}

void engine_ios_rotate_model(EngineIosHandle handle, int64_t modelId, float x, float y, float z)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->RotateModel(modelId, x, y, z);
}

void engine_ios_move_camera(EngineIosHandle handle, float delta)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->MoveCamera(delta);
}

void engine_ios_set_camera_position(EngineIosHandle handle, float x, float y, float z)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->SetCameraPosition(x, y, z);
}

void engine_ios_set_camera_rotation(EngineIosHandle handle, float yaw, float pitch, float roll)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->SetCameraRotation(yaw, pitch, roll);
}

void engine_ios_rotate_camera(EngineIosHandle handle, float yaw, float pitch, float roll)
{
	auto* pContext = toContext(handle);
	if (!pContext || !pContext->pCore)
	{
		return;
	}
	pContext->pCore->RotateCamera(yaw, pitch, roll);
}
#endif


