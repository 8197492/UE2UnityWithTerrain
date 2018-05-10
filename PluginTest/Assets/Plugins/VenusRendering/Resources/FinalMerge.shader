Shader "Hidden/FinalMerge"
{
	SubShader
	{
  		ZTest Off
        ZWrite Off
        Cull Off
		
		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag_final_merge
			#define FINAL_MERGE
			#define IMG_0 sceneRT
			#pragma multi_compile __ LUT_3D
			#pragma multi_compile __ HDR_ON
			#pragma multi_compile __ BLOOM_ON
			#include "PostFX.cginc"
			ENDCG
		}
	} 
	FallBack off
}
