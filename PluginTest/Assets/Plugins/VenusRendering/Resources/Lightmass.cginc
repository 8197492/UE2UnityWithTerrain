#if defined(USE_LIGHTMAP)

float4 _LightMapUVParams;
half4 _LightMapScale0;
half4 _LightMapAdd0;
half4 _LightMapScale1;
half4 _LightMapAdd1;
sampler2D _LightMap;

float2 lightmass_uv(in float2 base_uv)
{
	float2 uv = base_uv * _LightMapUVParams.xy + _LightMapUVParams.zw;
	return float2(uv.x, 1 - uv.y);
}

half4 lightmass_info_with_shadow(in float2 uv, in half3 N, in half penumbra, out half shadow)
{
	float2 uv1 = uv * float2(1, 0.5);
	float2 uv0 = uv1 + float2(0, 0.5);
	half4 lightMap0 = tex2D(_LightMap, uv0);
	half4 lightMap1 = tex2D(_LightMap, uv1);

	half distanceField = lightMap0.a;
	float distanceFieldBias = -0.5f * penumbra + 0.5f;
	half shadowFactor = saturate(distanceField * penumbra + distanceFieldBias);
	shadow = shadowFactor * shadowFactor;

	half3 logRGB = lightMap0.rgb * _LightMapScale0.xyz + _LightMapAdd0.xyz;
	half logL = dot(logRGB, half3(0.3, 0.59, 0.11));

	half l = exp2(logL * 16 - 8) - 0.00390625;

	half4 sh = lightMap1 * _LightMapScale1 + _LightMapAdd1;	// 1 vmad
	half directionality = max(0.0, dot(sh, half4(N.zy, -N.x, 1)));

	half luma = l * directionality;
	half3 color = logRGB * (luma / logL);				// 1 rcp, 1 smul, 1 vmul

	return half4(color, luma);
}

half4 lightmass_info(in float2 uv, in half3 N)
{
	float2 uv1 = uv * float2(1, 0.5);
	float2 uv0 = uv1 + float2(0, 0.5);
	half4 lightMap0 = tex2D(_LightMap, uv0);
	half4 lightMap1 = tex2D(_LightMap, uv1);

	half3 logRGB = lightMap0.rgb * _LightMapScale0.xyz + _LightMapAdd0.xyz;
	half logL = dot(logRGB, half3(0.3, 0.59, 0.11));

	half l = exp2(logL * 16 - 8) - 0.00390625;

	half4 sh = lightMap1 * _LightMapScale1 + _LightMapAdd1;	// 1 vmad
	half directionality = max(0.0, dot(sh, half4(N.zy, -N.x, 1)));

	half luma = l * directionality;
	half3 color = logRGB * (luma / logL);				// 1 rcp, 1 smul, 1 vmul

	return half4(color, luma);
}

#endif
