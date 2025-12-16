#pragma once

#include "GraphicsLayer.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace engine
{
	class CEngineCore
	{
	public:
		explicit CEngineCore(GraphicsLayerPtr pLayer);
		~CEngineCore();

		bool bInitialize(const SInitConfig& sConfig);
		void Resize(const SFrameConfig& sFrame);
		void Render();
		void Shutdown();

		void LoadModel(const SModelDescription& sDescription);
		void UpdateModelTransform(const SModelDescription& sDescription);
		void RemoveModel(int64_t llModelId);
		void TranslateModel(int64_t llModelId, float fX, float fY, float fZ);
		void ScaleModel(int64_t llModelId, float fScale);
		void RotateModel(int64_t llModelId, float fX, float fY, float fZ);

		void MoveCamera(float fDelta);
		void SetCameraPosition(float fX, float fY, float fZ);
		void SetCameraRotation(float fYaw, float fPitch, float fRoll);
		void RotateCamera(float fYaw, float fPitch, float fRoll);

	private:
		GraphicsLayerPtr m_pGraphicsLayer;
		SCameraState m_sCameraState{};
		std::mutex m_oMutex;
		bool m_bInitialized = false;
	};
}


