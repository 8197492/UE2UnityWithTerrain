#if defined(LANDSCAPE_1) || defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
#	define LANDSCAPE
#	define USE_LIGHTMAP
#	define NONMETAL
#endif

#if defined(LANDSCAPE_1) || defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
half4 _LayerMask0;
half4 _LayerParams0;
sampler2D _BaseMap0;
sampler2D _NormalMap0;
#endif

#if defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
half4 _LayerMask1;
half4 _LayerParams1;
sampler2D _BaseMap1;
sampler2D _NormalMap1;
#endif

#if defined(LANDSCAPE_3)
half4 _LayerMask2;
half4 _LayerParams2;
sampler2D _BaseMap2;
sampler2D _NormalMap2;
#endif

half4 landscape_base(sampler2D tex, float3 uv)
{
	half4 color = tex2D(tex, uv.xy);
	color.rgb *= color.rgb;
	return color * uv.z;
}

half4 landscape_normal(sampler2D tex, float3 uv)
{
	return tex2D(tex, uv.xy) * uv.z;
}
