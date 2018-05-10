// Unity built-in shader source. Copyright (c) 2016 Unity Technologies. MIT license (see license.txt)

Shader "Physical Shading/Particle/AddMask" {
Properties {
	_TintColor ("Tint Color", Color) = (1.0,1.0,1.0,1.0)
	_MainTex ("Particle Texture", 2D) = "white" {}
	_MaskTex("Mask Texture", 2D) = "white" {}
	_Params("(HDR,LDR,,)", Vector) = (1.0,1.0,1.0,1.0)
}

Category {
	Tags { "Queue"="Transparent" "IgnoreProjector"="True" "RenderType"="Transparent" "PreviewType"="Plane" }
	Blend SrcAlpha One
	ColorMask RGB
	Cull Off Lighting Off ZWrite Off
	
	SubShader {
		Pass {
			ZTest Less
		
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			#pragma multi_compile_particles
			#pragma multi_compile __ HDR_ON
			#pragma multi_compile __ POSTFX_ON
			#define MIX_ADD
			#define USE_MASK
			#include "Particle.cginc"
			ENDCG 
		}
	}	
}
}
