Shader "VenusRendering/Landscape/Landscape_1"
{
	Properties
	{
		_LayerMask0("Layer0 Mask", Vector) = (1, 0, 0, 0)
		_LayerParams0("Layer0 Params", Vector) = (0.1, 0, 0, 0.5)

		_BaseMap0("Layer0 Base Map", 2D) = "white" {}
		_NormalMap0("Layer0 Normal Map", 2D) = "white" {}
		
		_LightMap("Light Map", 2D) = "white" {}
		_Lighting("UVParams(sx,sy,bx,by)", Vector) = (1, 1, 0, 0)
		_LightMapUVParams("UVParams(sx,sy,bx,by)", Vector) = (1, 1, 0, 0)
		_LightMapScale0("Scale0(sx,sy,bx,by)", Vector) = (1, 1, 1, 0)
		_LightMapAdd0("Add0(sx,sy,bx,by)", Vector) = (0, 0, 0, 0)
		_LightMapScale1("Scale1(sx,sy,bx,by)", Vector) = (1, 1, 1, 0)
		_LightMapAdd1("Add1(sx,sy,bx,by)", Vector) = (0, 0, 0, 0)
	}

	SubShader
	{
		Tags
		{
			"RenderType" = "Opaque"
			"LightMode" = "ForwardBase"
			"Queue" = "Geometry"
		}
		LOD 100

		Pass
		{
			Cull Back
			ZTest Less
			ZWrite On

			CGPROGRAM
			#define LANDSCAPE_1
			#define DEPTH_OUTPUT
			#define USE_NORMAL_MAP
			#define USE_PBR
			#pragma vertex vs_standard
			#pragma fragment ps_standard
			#pragma multi_compile __ POSTFX_ON
			#pragma multi_compile __ HDR_ON
			#pragma multi_compile __ FOG_ON
			#pragma multi_compile __ SUNLIGHT_ON
			#pragma multi_compile __ GGX_ON
			#include "ForwardBasePass.cginc"
			ENDCG
		}

	}
}
