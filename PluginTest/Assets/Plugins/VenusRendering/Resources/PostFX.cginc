#include "UnityCG.cginc"

#ifdef IMG_0
sampler2D IMG_0;
#endif
#ifdef IMG_1
sampler2D IMG_1;
#endif
#ifdef IMG_2
sampler2D IMG_2;
#endif
#ifdef IMG_3
sampler2D IMG_3;
#endif

#ifdef PARAM_0
float4 PARAM_0;
#endif

#ifdef PARAM_1
float4 PARAM_1;
#endif

#ifdef PARAM_2
float4 PARAM_2;
#endif

#ifdef PARAM_3
float4 PARAM_3;
#endif

float2 Circle(float Start, float Points, float Point)
{
	float Rad = (3.141592653589 * 2.0 * (1.0 / Points)) * (Point + Start);
	return float2(sin(Rad), cos(Rad));
}

struct v2f
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0;
};

struct v2f_s4
{
	float4 pos : SV_POSITION;
	float4 uv01  : TEXCOORD0;
	float4 uv23  : TEXCOORD1;
};

struct v2f_s7
{
	float4 pos : SV_POSITION;
	float4 uv01 : TEXCOORD1;
	float4 uv23 : TEXCOORD2;
	float4 uv45 : TEXCOORD3;
	float2 uv6 : TEXCOORD4;
};

struct v2f_s15
{
	float4 pos : SV_POSITION;
	half4 uv01 : TEXCOORD0;
	half4 uv23 : TEXCOORD1;
	half4 uv45 : TEXCOORD2;
	half4 uv67 : TEXCOORD3;
	half4 uv89 : TEXCOORD4;
	half4 uv0A : TEXCOORD5;
	half4 uvBC : TEXCOORD6;
	half2 uvD : TEXCOORD7;
};

v2f vert(appdata_img v)
{
	v2f o;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv = v.vertex.xy;
	return o;
}

#if defined(PARAM_0)
v2f_s4 vert_s4(appdata_img v)
{
	v2f_s4 o;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv01.xy = v.texcoord.xy + PARAM_0 * float2(-1, -1);
	o.uv01.zw = v.texcoord.xy + PARAM_0 * float2(1, -1);
	o.uv23.xy = v.texcoord.xy + PARAM_0 * float2(-1, 1);
	o.uv23.zw = v.texcoord.xy + PARAM_0 * float2(1, 1);
	return o;
}

v2f_s15 vert_bloomdown(appdata_img v)
{
	v2f_s15 o;
	const float Start = 2.0 / 14.0;
	const float Scale = 2.64;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv01.xy = v.texcoord.xy;
	o.uv01.zw = v.texcoord.xy + Circle(Start, 14.0, 0.0) * Scale * PARAM_0.xy;
	o.uv23.xy = v.texcoord.xy + Circle(Start, 14.0, 1.0) * Scale * PARAM_0.xy;
	o.uv23.zw = v.texcoord.xy + Circle(Start, 14.0, 2.0) * Scale * PARAM_0.xy;
	o.uv45.xy = v.texcoord.xy + Circle(Start, 14.0, 3.0) * Scale * PARAM_0.xy;
	o.uv45.zw = v.texcoord.xy + Circle(Start, 14.0, 4.0) * Scale * PARAM_0.xy;
	o.uv67.xy = v.texcoord.xy + Circle(Start, 14.0, 5.0) * Scale * PARAM_0.xy;
	o.uv67.zw = v.texcoord.xy + Circle(Start, 14.0, 6.0) * Scale * PARAM_0.xy;
	o.uv89.xy = v.texcoord.xy + Circle(Start, 14.0, 7.0) * Scale * PARAM_0.xy;
	o.uv89.zw = v.texcoord.xy + Circle(Start, 14.0, 8.0) * Scale * PARAM_0.xy;
	o.uv0A.xy = v.texcoord.xy + Circle(Start, 14.0, 9.0) * Scale * PARAM_0.xy;
	o.uv0A.zw = v.texcoord.xy + Circle(Start, 14.0, 10.0) * Scale * PARAM_0.xy;
	o.uvBC.xy = v.texcoord.xy + Circle(Start, 14.0, 11.0) * Scale * PARAM_0.xy;
	o.uvBC.zw = v.texcoord.xy + Circle(Start, 14.0, 12.0) * Scale * PARAM_0.xy;
	o.uvD.xy = v.texcoord.xy + Circle(Start, 14.0, 13.0) * Scale * PARAM_0.xy;
	return o;
}
#endif

#if defined(PARAM_0) && defined(PARAM_1)
v2f_s15 vert_bloomup(appdata_img v)
{
	v2f_s15 o;
	const float Start = 2.0 / 7.0;
	const float Scale = 1.32;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv01.xy = v.texcoord.xy + Circle(Start, 7.0, 0.0) * Scale * PARAM_0.xy;
	o.uv01.zw = v.texcoord.xy + Circle(Start, 7.0, 1.0) * Scale * PARAM_0.xy;
	o.uv23.xy = v.texcoord.xy + Circle(Start, 7.0, 2.0) * Scale * PARAM_0.xy;
	o.uv23.zw = v.texcoord.xy + Circle(Start, 7.0, 3.0) * Scale * PARAM_0.xy;
	o.uv45.xy = v.texcoord.xy + Circle(Start, 7.0, 4.0) * Scale * PARAM_0.xy;
	o.uv45.zw = v.texcoord.xy + Circle(Start, 7.0, 5.0) * Scale * PARAM_0.xy;
	o.uv67.xy = v.texcoord.xy + Circle(Start, 7.0, 6.0) * Scale * PARAM_0.xy;
	o.uv67.zw = v.texcoord.xy;
	o.uv89.xy = v.texcoord.xy + Circle(Start, 7.0, 0.0) * Scale * PARAM_1.xy;
	o.uv89.zw = v.texcoord.xy + Circle(Start, 7.0, 1.0) * Scale * PARAM_1.xy;
	o.uv0A.xy = v.texcoord.xy + Circle(Start, 7.0, 2.0) * Scale * PARAM_1.xy;
	o.uv0A.zw = v.texcoord.xy + Circle(Start, 7.0, 3.0) * Scale * PARAM_1.xy;
	o.uvBC.xy = v.texcoord.xy + Circle(Start, 7.0, 4.0) * Scale * PARAM_1.xy;
	o.uvBC.zw = v.texcoord.xy + Circle(Start, 7.0, 5.0) * Scale * PARAM_1.xy;
	o.uvD.xy = v.texcoord.xy + Circle(Start, 7.0, 6.0) * Scale * PARAM_1.xy;
	return o;
}
#endif

#if defined(IMG_0)
half4 frag_scale_s1(v2f i) : SV_Target
{
	return tex2D(IMG_0, i.uv);
}
#endif

#if defined(IMG_0)
half4 frag_scale_s4(v2f_s4 i) : SV_Target
{
	half4 s = tex2D(IMG_0, i.uv01.xy);
	s += tex2D(IMG_0, i.uv01.zw);
	s += tex2D(IMG_0, i.uv23.xy);
	s += tex2D(IMG_0, i.uv23.zw);
	return s * 0.25;
}
#endif

#if defined(IMG_0) && defined(PARAM_0) && defined(PARAM_1)
half4 frag_bloom_setup(v2f_s4 i) : SV_Target
{
	half4 color;
	half4 C0 = tex2D(IMG_0, i.uv01.xy);
	half4 C1 = tex2D(IMG_0, i.uv01.zw);
	half4 C2 = tex2D(IMG_0, i.uv23.xy);
	half4 C3 = tex2D(IMG_0, i.uv23.zw);
	color.rgb = (C0.rgb * 0.25) + (C1.rgb * 0.25) + (C2.rgb * 0.25) + (C3.rgb * 0.25);
	color.rgb = max(color.rgb, 0);
	color.rgb = min(color.rgb, 5);
#ifdef HDR_ON
	half threshold = PARAM_1.z;
#else
	half threshold = PARAM_1.x;
#endif
	half amount = saturate(dot(color.rgb, half3(0.299, 0.587, 0.114)) - threshold);
	color.rgb *= amount;
	color.a = 0;

	return color;
}
#endif

#if defined(IMG_0) && defined(PARAM_0)
half4 frag_bloomdown(v2f_s15 i) : SV_Target
{
	half4 N0 = tex2D(IMG_0, i.uv01.xy);
	half4 N1 = tex2D(IMG_0, i.uv01.zw);
	half4 N2 = tex2D(IMG_0, i.uv23.xy);
	half4 N3 = tex2D(IMG_0, i.uv23.zw);
	half4 N4 = tex2D(IMG_0, i.uv45.xy);
	half4 N5 = tex2D(IMG_0, i.uv45.zw);
	half4 N6 = tex2D(IMG_0, i.uv67.xy);
	half4 N7 = tex2D(IMG_0, i.uv67.zw);
	half4 N8 = tex2D(IMG_0, i.uv89.xy);
	half4 N9 = tex2D(IMG_0, i.uv89.zw);
	half4 N10 = tex2D(IMG_0, i.uv0A.xy);
	half4 N11 = tex2D(IMG_0, i.uv0A.zw);
	half4 N12 = tex2D(IMG_0, i.uvBC.xy);
	half4 N13 = tex2D(IMG_0, i.uvBC.zw);
	half4 N14 = tex2D(IMG_0, i.uvD.xy);
#ifdef HQ_BLOOM
	half W = 1.0 / 15.0;
#else
	half W = 1.8 / 15.0;
#endif
	half4 color;
	color.rgb = (N0 * W) +
		(N1 * W) +
		(N2 * W) +
		(N3 * W) +
		(N4 * W) +
		(N5 * W) +
		(N6 * W) +
		(N7 * W) +
		(N8 * W) +
		(N9 * W) +
		(N10 * W) +
		(N11 * W) +
		(N12 * W) +
		(N13 * W) +
		(N14 * W);
	color.a = 0;
	return color;
}
#endif

#if defined(IMG_0) && defined(IMG_1) && defined(PARAM_0) && defined(PARAM_1) && defined(PARAM_2) && defined(PARAM_3)
half4 frag_bloomup(v2f_s15 i) : SV_Target
{
	half4 A0 = tex2D(IMG_0, i.uv01.xy);
	half4 A1 = tex2D(IMG_0, i.uv01.zw);
	half4 A2 = tex2D(IMG_0, i.uv23.xy);
	half4 A3 = tex2D(IMG_0, i.uv23.zw);
	half4 A4 = tex2D(IMG_0, i.uv45.xy);
	half4 A5 = tex2D(IMG_0, i.uv45.zw);
	half4 A6 = tex2D(IMG_0, i.uv67.xy);
	half4 A7 = tex2D(IMG_0, i.uv67.zw);

	half3 B0 = tex2D(IMG_1, i.uv67.zw);
	half3 B1 = tex2D(IMG_1, i.uv89.xy);
	half3 B2 = tex2D(IMG_1, i.uv89.zw);
	half3 B3 = tex2D(IMG_1, i.uv0A.xy);
	half3 B4 = tex2D(IMG_1, i.uv0A.zw);
	half3 B5 = tex2D(IMG_1, i.uvBC.xy);
	half3 B6 = tex2D(IMG_1, i.uvBC.zw);
	half3 B7 = tex2D(IMG_1, i.uvD.xy);

	half3 WA = PARAM_2.rgb;
	half3 WB = PARAM_3.rgb;

	half4 color;
	color.rgb =
		A0 * WA +
		A1 * WA +
		A2 * WA +
		A3 * WA +
		A4 * WA +
		A5 * WA +
		A6 * WA +
		A7 * WA +
		B0 * WB +
		B1 * WB +
		B2 * WB +
		B3 * WB +
		B4 * WB +
		B5 * WB +
		B6 * WB +
		B7 * WB;
	color.a = 0;

	return color;
}
#endif

#ifdef BLOOM_MERGE
#ifdef HQ_BLOOM
sampler2D bloomUp1;
float4 bloomUp1_TexelSize;
#define IMG_0 bloomUp1
#define PARAM_0 bloomUp1_TexelSize
#else
sampler2D bloomDown0;
float4 bloomDown0_TexelSize;
#define IMG_0 bloomDown0
#define PARAM_0 bloomDown0_TexelSize
#endif
v2f_s7 vert_bloom_merge(appdata_img v)
{
	v2f_s7 o;
	const float Start = 2.0 / 6.0;
	const float Scale = 0.66 / 2.0;
	o.pos = UnityObjectToClipPos(v.vertex);
	o.uv01.xy = v.texcoord.xy;
	o.uv01.zw = v.texcoord.xy + Circle(Start, 6.0, 0.0) * Scale * PARAM_0.xy;
	o.uv23.xy = v.texcoord.xy + Circle(Start, 6.0, 1.0) * Scale * PARAM_0.xy;
	o.uv23.zw = v.texcoord.xy + Circle(Start, 6.0, 2.0) * Scale * PARAM_0.xy;
	o.uv45.xy = v.texcoord.xy + Circle(Start, 6.0, 3.0) * Scale * PARAM_0.xy;
	o.uv45.zw = v.texcoord.xy + Circle(Start, 6.0, 4.0) * Scale * PARAM_0.xy;
	o.uv6 = v.texcoord.xy + Circle(Start, 6.0, 5.0) * Scale * PARAM_0.xy;
	return o;
}

half4 frag_bloom_merge(v2f_s7 i) : SV_Target
{
	const float W = 1.0 / 7.0;
	half4 N0 = tex2D(IMG_0, i.uv01.xy);
	half4 N1 = tex2D(IMG_0, i.uv01.zw);
	half4 N2 = tex2D(IMG_0, i.uv23.xy);
	half4 N3 = tex2D(IMG_0, i.uv23.zw);
	half4 N4 = tex2D(IMG_0, i.uv45.xy);
	half4 N5 = tex2D(IMG_0, i.uv45.zw);
	half4 N6 = tex2D(IMG_0, i.uv6);
	half4 color;
	color.rgb = (N0 * W) +
		(N1 * W) +
		(N2 * W) +
		(N3 * W) +
		(N4 * W) +
		(N5 * W) +
		(N6 * W);
	color.a = 0;
	return color;
}
#endif

#ifdef FINAL_MERGE
#ifdef HDR_ON
sampler2D bloomSetup;
float4 _HDRParams;
#ifdef LUT_3D
sampler3D colorHDRLUT3D;
#define colorLUT3D colorHDRLUT3D
#else
sampler2D colorHDRLUT;
#define colorLUT colorHDRLUT
#endif
#else
#ifdef BLOOM_ON
sampler2D bloomSetup;
float4 _BloomParams;
#endif
#ifdef LUT_3D
sampler3D colorLUT3D;
#else
sampler2D colorLUT;
#endif
#endif
half4 frag_final_merge(v2f i) : SV_Target
{
	half3 color = tex2D(IMG_0, i.uv).rgb;
#if defined(HDR_ON)
	color += tex2D(bloomSetup, i.uv).rgb * _HDRParams.y;
	color = 1 - exp(half3(-_HDRParams.z * color));
#elif defined(BLOOM_ON)
	color += tex2D(bloomSetup, i.uv).rgb * _BloomParams.y;
#endif
	color = sqrt(color);
#ifdef LUT_3D
	const half chartDim = 16.0;
	const half3 scale = half3(chartDim - 1.0, chartDim - 1.0, chartDim - 1.0) / chartDim;
	const half3 bias = half3(0.5, 0.5, 0.5) / chartDim;
	color = tex3D(colorLUT3D, color * scale + bias).rgb;
#else
	const half chartDim = 16.0;
	const half3 scale = half3(chartDim - 1.0, chartDim - 1.0, chartDim - 1.0) / chartDim;
	const half3 bias = half3(0.5, 0.5, 0.0) / chartDim;
	half3 lookup = color * scale + bias;
	half slice = lookup.z * chartDim;
	half sliceFrac = frac(slice);
	half sliceIdx = slice - sliceFrac;
	lookup.x = (lookup.x + sliceIdx) / chartDim;
	half3 col0 = tex2D(colorLUT, lookup.xy);
	lookup.x += 1.0 / chartDim;
	half3 col1 = tex2D(colorLUT, lookup.xy);
	color = col0 + (col1 - col0) * sliceFrac;
#endif
	return half4(color, 1);
}
#endif
