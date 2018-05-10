Shader "Physical Shading/Show/PBRGlow"
{
	Properties
	{
		_MainTex("Base Texture", 2D) = "white" {}
		_NrmTex("Normal Map", 2D) = "white" {}
		_MixTex("Mix Map", 2D) = "white" {}
		
		_GlowTex("Glow Map", 2D) = "black" {}
		_GlowIntensity("Glow Intensity", Range(0, 10)) = 1.0

		_EnvTex("Environment Map", CUBE) = "white" {}
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
			#define DEPTH_OUTPUT
			#define USE_NORMAL_MAP
			#define USE_PBR
			#define USE_GLOW
			#define USE_SPEC_MAP
			#define USE_SHOW
			#define ENV_MIP_NUM 9
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
