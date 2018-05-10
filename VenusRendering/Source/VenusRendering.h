#pragma once

#include "Venus3D.h"
#include "Unity/IUnityInterface.h"
#include "Unity/IUnityGraphics.h"

class VenusRendering
{
public:
	static void Init(IUnityInterfaces* pkUnityInterfaces);

	static void Term();

	static void Log(const char* pcMessage);

	static VenusRendering* GetInstance();

	UnityGfxRenderer GetDeviceType();

	void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

private:
	VenusRendering(IUnityInterfaces* pkUnityInterfaces);

	~VenusRendering();

	

	IUnityGraphics* m_pkGraphics = nullptr;
	UnityGfxRenderer m_eDeviceType = kUnityGfxRendererNull;

	static VenusRendering* ms_pkSingleton;

};
