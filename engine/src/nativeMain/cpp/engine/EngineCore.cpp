#include "EngineCore.h"
#include <utility>

namespace engine
{
	CEngineCore::CEngineCore(GraphicsLayerPtr pLayer)
		: m_pGraphicsLayer(std::move(pLayer))
	{
	}

	CEngineCore::~CEngineCore()
	{
		Shutdown();
	}

	bool CEngineCore::bInitialize(const SInitConfig& sConfig)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_pGraphicsLayer)
		{
			return false;
		}
		if (!m_pGraphicsLayer->bInitialize(sConfig))
		{
			return false;
		}
		m_pGraphicsLayer->InitializeResources();
		m_bInitialized = true;
		return true;
	}

	void CEngineCore::Resize(const SFrameConfig& sFrame)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->Resize(sFrame);
	}

	void CEngineCore::Render()
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->RenderFrame(m_sCameraState);
	}

	void CEngineCore::Shutdown()
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->Shutdown();
		m_bInitialized = false;
	}

	void CEngineCore::LoadModel(const SModelDescription& sDescription)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->LoadModel(sDescription);
	}

	void CEngineCore::UpdateModelTransform(const SModelDescription& sDescription)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->UpdateModelTransform(sDescription);
	}

	void CEngineCore::RemoveModel(int64_t llModelId)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->RemoveModel(llModelId);
	}

	void CEngineCore::TranslateModel(int64_t llModelId, float fX, float fY, float fZ)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->TranslateModel(llModelId, fX, fY, fZ);
	}

	void CEngineCore::ScaleModel(int64_t llModelId, float fScale)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->ScaleModel(llModelId, fScale);
	}

	void CEngineCore::RotateModel(int64_t llModelId, float fX, float fY, float fZ)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->RotateModel(llModelId, fX, fY, fZ);
	}

	void CEngineCore::MoveCamera(float fDelta)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_pGraphicsLayer->MoveCamera(fDelta);
		m_sCameraState.fPosition[2] -= fDelta;
	}

	void CEngineCore::SetCameraPosition(float fX, float fY, float fZ)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_sCameraState.fPosition[0] = fX;
		m_sCameraState.fPosition[1] = fY;
		m_sCameraState.fPosition[2] = fZ;
		m_pGraphicsLayer->SetCameraPosition(m_sCameraState);
	}

	void CEngineCore::SetCameraRotation(float fYaw, float fPitch, float fRoll)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_sCameraState.fRotation[0] = fYaw;
		m_sCameraState.fRotation[1] = fPitch;
		m_sCameraState.fRotation[2] = fRoll;
		m_pGraphicsLayer->SetCameraRotation(m_sCameraState);
	}

	void CEngineCore::RotateCamera(float fYaw, float fPitch, float fRoll)
	{
		std::lock_guard<std::mutex> oGuard(m_oMutex);
		if (!m_bInitialized || !m_pGraphicsLayer)
		{
			return;
		}
		m_sCameraState.fRotation[0] += fYaw;
		m_sCameraState.fRotation[1] += fPitch;
		m_sCameraState.fRotation[2] += fRoll;
		m_pGraphicsLayer->RotateCamera(fYaw, fPitch, fRoll);
	}
}


