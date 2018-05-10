Shader "Hidden/Bloom"
{
	SubShader
	{
  		ZTest Off
        ZWrite Off
        Cull Off
		
		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_s4
	    	#pragma fragment frag_bloom_setup
			#pragma multi_compile __ HDR_ON
			#pragma multi_compile __ MSAA_ON
			#define IMG_0 sceneRT
			#define PARAM_0 sceneRT_TexelSize
			#define PARAM_1 _BloomParams
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloomdown
	    	#pragma fragment frag_bloomdown
			#pragma multi_compile __ HQ_BLOOM
			#define IMG_0 bloomSetup
			#define PARAM_0 bloomSetup_TexelSize
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloomdown
	    	#pragma fragment frag_bloomdown
			#define HQ_BLOOM
			#define IMG_0 bloomDown0
			#define PARAM_0 bloomDown0_TexelSize
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloomdown
	    	#pragma fragment frag_bloomdown
			#define HQ_BLOOM
			#define IMG_0 bloomDown1
			#define PARAM_0 bloomDown1_TexelSize
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloomup
	    	#pragma fragment frag_bloomup
			#define HQ_BLOOM
			#define IMG_0 bloomDown2
			#define IMG_1 bloomDown1
			#define PARAM_0 bloomDown2_TexelSize
			#define PARAM_1 bloomDown1_TexelSize
			#define PARAM_2 bloomHint0
			#define PARAM_3 bloomHint1
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloomup
	    	#pragma fragment frag_bloomup
			#define HQ_BLOOM
			#define IMG_0 bloomUp0
			#define IMG_1 bloomDown0
			#define PARAM_0 bloomUp0_TexelSize
			#define PARAM_1 bloomDown0_TexelSize
			#define PARAM_2 bloomHint2
			#define PARAM_3 bloomHint3
			#include "PostFX.cginc"		
	    	ENDCG
	  	}

		Pass
		{
	    	CGPROGRAM
	    	#pragma vertex vert_bloom_merge
	    	#pragma fragment frag_bloom_merge
			#define BLOOM_MERGE
			#pragma multi_compile __ HQ_BLOOM
			#include "PostFX.cginc"		
	    	ENDCG
	  	}
	}
	FallBack off
}
