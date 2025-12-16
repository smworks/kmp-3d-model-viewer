#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
	struct SModelTransform
	{
		float fPosition[3] = {0.0f, 0.0f, 0.0f};
		float fScale = 1.0f;
		float fRotation[3] = {0.0f, 0.0f, 0.0f};
	};

	struct SModelDescription
	{
		int64_t llId = 0;
		std::string sAssetPath;
		SModelTransform sTransform;
	};

	struct SCameraState
	{
		float fPosition[3] = {0.0f, 0.0f, 4.0f};
		float fRotation[3] = {0.0f, 0.0f, 0.0f};
	};

	struct SFrameConfig
	{
		uint32_t uWidth = 0;
		uint32_t uHeight = 0;
	};

	struct SInitConfig
	{
		void* pNativeWindow = nullptr;
		void* pPlatformHandle = nullptr;
		uint32_t uWidth = 0;
		uint32_t uHeight = 0;
	};

	class CGraphicsLayer
	{
	public:
		virtual ~CGraphicsLayer() = default;

		virtual bool bInitialize(const SInitConfig& sConfig) = 0;
		virtual void InitializeResources() = 0;
		virtual void Resize(const SFrameConfig& sFrame) = 0;
		virtual void RenderFrame(const SCameraState& sCamera) = 0;
		virtual void Shutdown() = 0;

		virtual void LoadModel(const SModelDescription& sModel) = 0;
		virtual void UpdateModelTransform(const SModelDescription& sModel) = 0;
		virtual void RemoveModel(int64_t llModelId) = 0;
		virtual void TranslateModel(int64_t llModelId, float fX, float fY, float fZ) = 0;
		virtual void ScaleModel(int64_t llModelId, float fScale) = 0;
		virtual void RotateModel(int64_t llModelId, float fX, float fY, float fZ) = 0;

		virtual void MoveCamera(float fDelta) = 0;
		virtual void SetCameraPosition(const SCameraState& sCamera) = 0;
		virtual void SetCameraRotation(const SCameraState& sCamera) = 0;
		virtual void RotateCamera(float fYaw, float fPitch, float fRoll) = 0;
	};

	using GraphicsLayerPtr = std::unique_ptr<CGraphicsLayer>;
	using GraphicsLayerFactory = GraphicsLayerPtr (*)();

#ifdef __APPLE__
	class CIosGraphicsLayer final : public CGraphicsLayer
	{
	public:
		bool bInitialize(const SInitConfig&) override { return true; }
		void InitializeResources() override {}
		void Resize(const SFrameConfig&) override {}
		void RenderFrame(const SCameraState&) override {}
		void Shutdown() override {}
		void LoadModel(const SModelDescription&) override {}
		void UpdateModelTransform(const SModelDescription&) override {}
		void RemoveModel(int64_t) override {}
		void TranslateModel(int64_t, float, float, float) override {}
		void ScaleModel(int64_t, float) override {}
		void RotateModel(int64_t, float, float, float) override {}
		void MoveCamera(float) override {}
		void SetCameraPosition(const SCameraState&) override {}
		void SetCameraRotation(const SCameraState&) override {}
		void RotateCamera(float, float, float) override {}
	};
#endif
}


