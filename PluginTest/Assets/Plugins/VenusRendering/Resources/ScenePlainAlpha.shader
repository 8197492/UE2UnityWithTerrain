Shader "Physical Shading/Scene/PlainAlpha"
{
	Properties
	{
		_MainTex("Base Texture", 2D) = "white" {}
		_LightMap("Light Map", 2D) = "white" {}
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
			"RenderType" = "TransparentCutout"
			"LightMode" = "ForwardBase"
			"Queue" = "AlphaTest"
		}
		LOD 100

		Pass
		{
			Cull Back
			ZTest Less
			ZWrite On

			CGPROGRAM
			#define DEPTH_OUTPUT
			#define USE_LIGHTMAP
			#define USA_ALPHA_MASK
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
