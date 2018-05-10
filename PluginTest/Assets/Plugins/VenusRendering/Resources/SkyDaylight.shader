Shader "VenusRendering/Skybox/Daylight"
{
	Properties
	{
		_SunSize("Sun Size", Range(0.03,0.06)) = 0.04
		_AtmosphereThickness("Atmosphere Thickness", Range(0,5)) = 1.0
		_Exposure("Exposure", Range(0, 8)) = 1

		_SkyTint("Sky Tint", Color) = (0.8, 0.8, 0.8, 1)
		_SkyColor("Sky Color", Color) = (1, 1, 1, 1)
		_FogParams("Fog Params", Vector) = (0.5, 0.2, 2, 0.5)
		_CloudParams("Cloud Params", Vector) = (0.3, 0.1, 1, 0.25)

		_CloudTex("Cloud Map", 2D) = "white" {}
	}

	SubShader
	{
		Tags
		{
			"Queue"="Background"
			"RenderType"="Background"
			"PreviewType"="Skybox"
		}
		Cull Off ZWrite Off

		Pass
		{

			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			#pragma multi_compile __ POSTFX_ON
			#pragma multi_compile __ FOG_ON
			#include "SkyDome.cginc"
			ENDCG
		}
	}

	Fallback Off
}
