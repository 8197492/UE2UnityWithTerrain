#include "VenusRendering.h"

extern "C"
{
	typedef void(*LogFunc)(const char*);

	LogFunc g_funcLogFunction = nullptr;

	int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetTestInteger()
	{
		if (VenusRendering::GetInstance())
		{
			return VenusRendering::GetInstance()->GetDeviceType();
		}
		return kUnityGfxRendererNull;
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitLogFunc(LogFunc funcLog)
	{
		g_funcLogFunction = funcLog;
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API TermLogFunc()
	{
		g_funcLogFunction = nullptr;
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* pkUnityInterfaces)
	{
		VenusRendering::Init(pkUnityInterfaces);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
	{
		VenusRendering::Term();
	}
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	if (VenusRendering::GetInstance())
	{
		VenusRendering::GetInstance()->OnGraphicsDeviceEvent(eventType);
	}
}

VenusRendering* VenusRendering::ms_pkSingleton = nullptr;

void VenusRendering::Init(IUnityInterfaces* pkUnityInterfaces)
{
	if (!ms_pkSingleton)
	{
		ms_pkSingleton = new VenusRendering(pkUnityInterfaces);

	}
}

void VenusRendering::Term()
{
	if (ms_pkSingleton)
	{
		delete ms_pkSingleton;
		ms_pkSingleton = nullptr;
	}
}

void VenusRendering::Log(const char* pcMessage)
{
	if (g_funcLogFunction && pcMessage)
	{
		g_funcLogFunction(pcMessage);
	}
}

VenusRendering* VenusRendering::GetInstance()
{
	return ms_pkSingleton;
}

UnityGfxRenderer VenusRendering::GetDeviceType()
{
	return m_eDeviceType;
}

void VenusRendering::OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		m_eDeviceType = m_pkGraphics->GetRenderer();
	}

	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		m_eDeviceType = kUnityGfxRendererNull;
	}
}

VenusRendering::VenusRendering(IUnityInterfaces* pkUnityInterfaces)
{
	m_pkGraphics = pkUnityInterfaces->Get<IUnityGraphics>();
	m_pkGraphics->RegisterDeviceEventCallback(::OnGraphicsDeviceEvent);
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

VenusRendering::~VenusRendering()
{
	m_pkGraphics->UnregisterDeviceEventCallback(::OnGraphicsDeviceEvent);
}
