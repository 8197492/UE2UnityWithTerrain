#include "Venus3D.h"
#include "Unity/IUnityInterface.h"
#include "Unity/IUnityGraphics.h"

static IUnityInterfaces* s_pkUnityInterfaces = nullptr;
static IUnityGraphics* s_pkGraphics = nullptr;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

extern "C"
{
	

	static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
	{
		switch (eventType)
		{
		case kUnityGfxDeviceEventInitialize:
			s_DeviceType = s_pkGraphics->GetRenderer();
			break;
		case kUnityGfxDeviceEventShutdown:
			s_DeviceType = kUnityGfxRendererNull;
			break;
		case kUnityGfxDeviceEventBeforeReset:
			break;
		case kUnityGfxDeviceEventAfterReset:
			break;
		default:
			break;
		}
	}

	
}
