#pragma once

#include <jni.h>
#include "engine/GraphicsLayer.h"

namespace engine
{
	class CGraphicsEngine final : public CGraphicsLayer
	{
	public:
		CGraphicsEngine();
		~CGraphicsEngine() override;

		bool bInitialize(const SInitConfig& sConfig) override;
		void InitializeResources() override;
		void Resize(const SFrameConfig& sFrame) override;
		void RenderFrame(const SCameraState& sCamera) override;
		void Shutdown() override;

		void LoadModel(const SModelDescription& sModel) override;
		void UpdateModelTransform(const SModelDescription& sModel) override;
		void RemoveModel(int64_t llModelId) override;
		void TranslateModel(int64_t llModelId, float fX, float fY, float fZ) override;
		void ScaleModel(int64_t llModelId, float fScale) override;
		void RotateModel(int64_t llModelId, float fX, float fY, float fZ) override;

		void MoveCamera(float fDelta) override;
		void SetCameraPosition(const SCameraState& sCamera) override;
		void SetCameraRotation(const SCameraState& sCamera) override;
		void RotateCamera(float fYaw, float fPitch, float fRoll) override;
	};

	void SetJavaBindings(JavaVM* pVm, jclass engineApiClass, jmethodID loadFileMethod);
}
