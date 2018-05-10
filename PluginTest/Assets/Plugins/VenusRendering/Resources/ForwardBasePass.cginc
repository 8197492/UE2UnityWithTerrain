#include "UnityCG.cginc"
#include "Common.cginc"
#include "BRDF.cginc"
#include "Landscape.cginc"
#include "Lightmass.cginc"
#include "Fog.cginc"

struct vs_input
{
	float4 vertex		: POSITION;
	float3 normal		: NORMAL;
#if defined(LANDSCAPE)
	fixed4 color : COLOR;
#else
#	if defined(USE_NORMAL_MAP) || defined(USE_HAIR)
	half4 tangent			: TANGENT;
#	endif
	float2 uv				: TEXCOORD0;
#	ifdef USE_LIGHTMAP
	float2 uv1				: TEXCOORD3;
#	endif
#endif
};

struct vs_output
{
	float4 vertex			: SV_POSITION;
#if defined(DEPTH_OUTPUT)
	half4 normal			: NORMAL;
#else
	half3 normal			: NORMAL;
#endif
	
#if defined(LANDSCAPE_1)
	float2 uv				: TEXCOORD0;
	float3 layer0			: TEXCOORD1;
	half3 viewDir			: TEXCOORD2;
#	if defined(USE_WORLD_POS)
	float3 worldPos			: TEXCOORD3;
#	endif
#elif defined(LANDSCAPE_2)
	float2 uv				: TEXCOORD0;
	float3 layer0			: TEXCOORD1;
	float3 layer1			: TEXCOORD2;
	half3 viewDir			: TEXCOORD3;
#	if defined(USE_WORLD_POS)
	float3 worldPos			: TEXCOORD4;
#	endif
#elif defined(LANDSCAPE_3)
	float2 uv				: TEXCOORD0;
	float3 layer0			: TEXCOORD1;
	float3 layer1			: TEXCOORD2;
	float3 layer2			: TEXCOORD3;
	half3 viewDir			: TEXCOORD4;
#	if defined(USE_WORLD_POS)
	float3 worldPos			: TEXCOORD5;
#	endif
#else
#	if defined(USE_NORMAL_MAP) || defined(USE_HAIR)
	half4 tangent			: TANGENT;
#	endif
#	ifdef USE_LIGHTMAP
	float4 uv				: TEXCOORD0;
#	else
	float2 uv				: TEXCOORD0;
#	endif
	half3 viewDir			: TEXCOORD1;
#	if defined(USE_SCREEN_UV)
	float4 projPos			: TEXCOORD2;
#	if defined(USE_WORLD_POS)
	float3 worldPos			: TEXCOORD3;
#	endif
#	elif defined(USE_WORLD_POS)
	float3 worldPos			: TEXCOORD2;
#	endif
#endif
#if defined(FOG_ON)
	fixed4 fogColor : COLOR;
#endif
};

vs_output vs_standard(vs_input i)
{
	vs_output o;
	float4 wPos = mul(unity_ObjectToWorld, i.vertex);
	o.vertex = mul(UNITY_MATRIX_VP, wPos);
	o.normal.xyz = UnityObjectToWorldNormal(i.normal);
#if defined(LANDSCAPE)
	o.uv = lightmass_uv(i.vertex.xz);
	o.layer0.xy = float2(-wPos.x, -wPos.z) * _LayerParams0.x + float2(0, 1);
	o.layer0.z = dot(i.color, _LayerMask0);
#	if defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
	o.layer1.xy = float2(-wPos.x, -wPos.z) * _LayerParams1.x + float2(0, 1);
	o.layer1.z = dot(i.color, _LayerMask1);
#	endif
#	if defined(LANDSCAPE_3)
	o.layer2.xy = float2(-wPos.x, -wPos.z) * _LayerParams2.x + float2(0, 1);
	o.layer2.z = dot(i.color, _LayerMask2);
#	endif
#else
#	if defined(USE_NORMAL_MAP) || defined(USE_HAIR)
	o.tangent.xyz = UnityObjectToWorldDir(i.tangent.xyz);
	o.tangent.w = i.tangent.w;
#	endif
	o.uv.xy = i.uv;
#	ifdef USE_LIGHTMAP
	o.uv.zw = lightmass_uv(float2(i.uv1.x, 1 - i.uv1.y));
#	endif
#endif
#if defined(DEPTH_OUTPUT)
	o.normal.w = o.vertex.w;
#endif
	o.viewDir = _WorldSpaceCameraPos - wPos.xyz;
#ifdef USE_SCREEN_UV
	o.projPos = ComputeScreenPos(o.vertex);
#endif
#if defined(USE_WORLD_POS)
	o.worldPos = wPos;
#endif
#if defined(FOG_ON)
	o.fogColor = GetExponentialHeightFog(wPos.xyz - _WorldSpaceCameraPos);
#endif
	return o;
}

#ifndef LANDSCAPE

#ifdef USE_SHOW
samplerCUBE _EnvTex;
#endif

sampler2D _MainTex;

#ifdef USE_NORMAL_MAP
sampler2D _NrmTex;
#endif

#if defined(USE_PBR) || defined(USE_HAIR)
sampler2D _MixTex;
#endif

#if defined(USE_PLANAR) && defined(PLANAR_ON)
sampler2D mirrorShadowRT;
#endif

#if (!defined(USE_SPEC_MAP)) || defined(USE_SKIN_SCATTER)
half _Specular;
#endif

#ifdef USE_SKIN_SCATTER
half4 _SkinParams;
half3 _SkinScatter;
#endif

#ifdef USE_GLOW
sampler2D _GlowTex;
half _GlowIntensity;
#endif

#ifdef USE_FOCUS
half4 _FocusColor;
#endif

#ifdef USE_DISSOLVE
half3 _Dissolve;
half3 _DissolveColor;
#endif

#ifdef USE_TERRAIN
sampler2D _BaseLayer0;
sampler2D _BaseLayer1;
sampler2D _Blend;
float3 _Tiling;
#endif

#ifdef USE_ROLE
half3 _roleLighting;
#endif

#endif

half4 ps_standard(vs_output i) : SV_Target
{
	half3 N = normalize(i.normal.xyz);
#if defined(USE_NORMAL_MAP) && defined(LANDSCAPE)
	half4 normalMapColor = landscape_normal(_NormalMap0, i.layer0);
#	if defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
	normalMapColor += landscape_normal(_NormalMap1, i.layer1);
#	endif
#	if defined(LANDSCAPE_3)
	normalMapColor += landscape_normal(_NormalMap2, i.layer2);
#	endif
	half3 T = normalize(float3(-N.y, N.x, 0));
	half3 B = cross(N, T);
	N = normal_map(normalMapColor.rgb, half3x3(T, B, N));
#elif defined(USE_NORMAL_MAP)
	half3 T = normalize(i.tangent.xyz);
	half3 B = cross(N, T) * i.tangent.w;
#	ifdef USE_TERRAIN
	half3 normalMapColor = lerp(half3(0.5, 0.5, 1.0), tex2D(_NrmTex, i.uv.xy * _Tiling.x).rgb, blend.r);
#	else
	half3 normalMapColor = tex2D(_NrmTex, i.uv.xy).rgb;
	normalMapColor.g = 1.0 - normalMapColor.g;
#	endif
	N = normal_map(normalMapColor.rgb, half3x3(T, B, N));
#endif

#if defined(LANDSCAPE)
	half4 baseColor = landscape_base(_BaseMap0, i.layer0);
#	if defined(LANDSCAPE_2) || defined(LANDSCAPE_3)
	baseColor += landscape_base(_BaseMap1, i.layer1);
#	endif
#	if defined(LANDSCAPE_3)
	baseColor += landscape_base(_BaseMap2, i.layer2);
#	endif
#else
	half4 baseColor = 0;
#	ifdef USE_TERRAIN
	half4 blend = tex2D(_Blend, i.uv.xy);
	baseColor.rgb += blend.r * tex2D(_MainTex, i.uv.xy * _Tiling.x);
	baseColor.rgb += blend.g * tex2D(_BaseLayer0, i.uv.xy * _Tiling.y);
	baseColor.rgb += blend.b * tex2D(_BaseLayer1, i.uv.xy * _Tiling.z);
	baseColor.a = 1;
#	else
	baseColor = tex2D(_MainTex, i.uv.xy);
#	endif
	baseColor.rgb *= baseColor.rgb;
#	if defined(USE_DISSOLVE)
	half disNoise = min((cnoise(i.worldPos * _Dissolve.x) + 1.) * 0.5, 0.999) - _Dissolve.y;
	baseColor.a *= disNoise + 0.5;
	clip(baseColor.a - 0.5);
	if (_Dissolve.z > 0)
	{
		disNoise = smoothstep(0, _Dissolve.z, disNoise);
	}
	else
	{
		disNoise = 1;
	}
#	elif defined(USA_ALPHA_MASK)
	clip(baseColor.a - 0.5);
#	endif
#endif

#if defined(USE_NOV)
	half3 V = normalize(i.viewDir);
	half NoV = max(dot(N, V), 0);
#endif

#if defined(USE_PBR)
	half3 R = normalize(reflect(-V, N));
	half roughness;
#	if defined(LANDSCAPE)
	roughness = baseColor.a;
#	else
#	ifdef USE_TERRAIN
	half3 mixColor = lerp(half3(0, 1, 0), tex2D(_MixTex, i.uv.xy * _Tiling.x).rgb, blend.r);
#	else
	half3 mixColor = tex2D(_MixTex, i.uv.xy).rgb;
#	endif
	roughness = mixColor.g;
#	endif
#	if defined(NONMETAL)
	half3 diffuseColor = baseColor.rgb;
	half specularColor = 0.04;
	half specularColorEnv = EnvBRDFApproxNonmetal(roughness, NoV);
#	else
#	if defined(LANDSCAPE)
	half specular = 0.5;
	half metallic = 0;
#	else
#	if defined(USE_SPEC_MAP) && (!defined(USE_SKIN_SCATTER))
	half specular = mixColor.r;
#	else
	half specular = _Specular;
#	endif
	half metallic = mixColor.b;
#	endif
	half dielectricSpecular = 0.08 * specular;
	half3 diffuseColor = baseColor.rgb - baseColor.rgb * metallic;	// 1 mad
	half3 specularColor = (dielectricSpecular - dielectricSpecular * metallic) + baseColor * metallic;	// 2 mad
	half3 specularColorEnv = EnvBRDFApprox(specularColor, roughness, NoV);
#	endif
#else
	half3 diffuseColor = baseColor;
#endif

	half3 color = 0;
	half irradiance = 0;
	half shadow = 1;

#if defined(USE_LIGHTMAP)

#	if defined(LANDSCAPE)
	float2 lightmap_uv = i.uv;
#	else
	float2 lightmap_uv = i.uv.zw;
#	endif

#	if defined(SUNLIGHT_ON)
	half4 lightInfo = lightmass_info_with_shadow(lightmap_uv, N, _sunLightPenumbra.x, shadow);
#	else
	half4 lightInfo = lightmass_info(lightmap_uv, N);
#	endif
	//return lightInfo;
	color += lightInfo.rgb * diffuseColor;
	irradiance += lightInfo.a;

#elif defined(USE_SH_VOLUME)

#elif defined(USE_SHOW)

	half3 diffuseGI = RGBMDecode(texCUBElod(_EnvTex, half4(N, ENV_MIP_NUM)));
	color += diffuseColor * diffuseGI;
	irradiance += GetLuminance(diffuseGI);

#else

	half3 diffuseGI = (N.y * 0.5 + 0.5);
#	ifdef USE_ROLE
	diffuseGI *= _roleLighting.x;
#	endif
	color += diffuseColor * diffuseGI;
	irradiance += GetLuminance(diffuseGI);

#endif

#if defined(USE_PLANAR) && defined(PLANAR_ON)
	half4 mirrorShadow = tex2Dproj(mirrorShadowRT, UNITY_PROJ_COORD(i.projPos));
	shadow *= mirrorShadow.a;
	color *= lerp(0.7, 1, mirrorShadow.a);
#endif

	half3 L = main_light_dir();
	half3 LC = main_light_color();
#ifdef USE_ROLE
	LC *= _roleLighting.z;
#endif
#ifdef USE_SHOW
	L = normalize(_WorldSpaceLightPos0.xyz);
#endif

	half NoL = max(0, dot(N, L));

#if defined(USE_PBR)
	roughness = lerp(0.05, 1, roughness);
	half RoL = max(0, dot(R, L));
	
#	if defined(GGX_ON)
	half3 H = normalize(V + L);
	half NoH = max(0, dot(N, H));
	half VoH = max(0, dot(V, H));
	half D = D_GGX(roughness, NoH);
	half Vis = Vis_SmithJointApprox(roughness, NoV, NoL);
	half3 F = F_Schlick(specularColor, VoH);
	half3 LE = (NoL * shadow) * LC;
	color += LE * diffuseColor;
	color += max((D * Vis * PI) * F * LE, 0);
#	else
	color += (NoL * shadow) * LC * (diffuseColor + specularColorEnv * PhongApprox(roughness, RoL));
#	endif

	half absoluteSpecularMip = ComputeReflectionCaptureMipFromRoughness(roughness);
#	ifdef USE_SHOW
	half3 specularIBL = RGBMDecode(texCUBElod(_EnvTex, half4(R, absoluteSpecularMip)));
#	else
	half3 projectedCaptureVector = GetSphereCaptureVector(half3(-R.x, R.yz), i.worldPos,
		half4(unity_SpecCube0_ProbePosition.xyz, 0.5 * (unity_SpecCube0_BoxMax.x - unity_SpecCube0_BoxMin.x)));
	half3 specularIBL = RGBMDecode(UNITY_SAMPLE_TEXCUBE_LOD(unity_SpecCube0, projectedCaptureVector, absoluteSpecularMip)) * unity_SpecCube0_HDR.x;
#	endif
	specularIBL *= irradiance;
#	ifdef USE_ROLE
	specularIBL *= _roleLighting.y;
#	endif
	color += specularIBL * specularColorEnv;
#else
	color += NoL * LC * diffuseColor;
#endif

#ifdef USE_GLOW
	half3 glowColor = tex2D(_GlowTex, i.uv.xy).rgb;
	color += glowColor * glowColor * _GlowIntensity;
#endif

#ifdef USE_FOCUS
	color += pow((1 - NoV), _FocusColor.w) * _FocusColor.rgb;
#endif

#ifdef USE_DISSOLVE
	color = lerp(_DissolveColor, color, disNoise);
#endif

#if defined(FOG_ON)
	color = color * i.fogColor.a + i.fogColor.rgb;
#endif

#if defined(UI_DISPLAY) || (!defined(POSTFX_ON))
	color = sqrt(color);
#endif

#if defined(DEPTH_OUTPUT)
	return half4(color, i.normal.w);
#else
	return half4(color, 1);
#endif
}
