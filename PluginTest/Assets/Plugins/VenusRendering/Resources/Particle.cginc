#include "UnityCG.cginc"

fixed4 _TintColor;

struct appdata_t {
	float4 vertex : POSITION;
	fixed4 color : COLOR;
#ifdef EDGE_HIGHLIGHT
	half3 normal			: NORMAL;
#endif
	float2 texcoord : TEXCOORD0;
	UNITY_VERTEX_INPUT_INSTANCE_ID
};

struct v2f {
	float4 vertex : SV_POSITION;
	fixed4 color : COLOR;
#	if defined(DISSOLVE) || defined(USE_MASK)
	float4 texcoord : TEXCOORD0;
#	else
	float2 texcoord : TEXCOORD0;
#	endif
#ifdef EDGE_HIGHLIGHT
	half3 normal			: NORMAL;
	half3 viewDir			: TEXCOORD1;
#endif
};

sampler2D _MainTex;
float4 _MainTex_ST;
#ifdef USE_MASK
sampler2D _MaskTex;
float4 _MaskTex_ST;
#endif
#ifdef DISSOLVE
sampler2D _DissolveTex;
float4 _DissolveTex_ST;
#endif
#ifdef GLOW
sampler2D _GlowTex;
#endif

v2f vert(appdata_t v)
{
	v2f o;
	UNITY_SETUP_INSTANCE_ID(v);
	o.vertex = UnityObjectToClipPos(v.vertex);
	o.color = v.color * _TintColor;
	o.texcoord.xy = TRANSFORM_TEX(v.texcoord, _MainTex);
#	if defined(DISSOLVE)
	o.texcoord.zw = TRANSFORM_TEX(v.texcoord, _DissolveTex);
#	elif defined(USE_MASK)
	o.texcoord.zw = TRANSFORM_TEX(v.texcoord, _MaskTex);
#	endif
#ifdef EDGE_HIGHLIGHT
	float4 wPos = mul(unity_ObjectToWorld, v.vertex);
	o.normal = UnityObjectToWorldNormal(v.normal);
	o.viewDir = _WorldSpaceCameraPos - wPos.xyz;
#endif
	return o;
}

half4 _Params;
#ifdef EDGE_HIGHLIGHT
half4 _EdgeTintColor;
half4 _EdgeParams;
#endif

fixed4 frag(v2f i) : SV_Target
{
	half4 col = i.color * tex2D(_MainTex, i.texcoord.xy);
	col.rgb *= col.rgb;
#	ifdef GLOW
	half3 glow = tex2D(_GlowTex, i.texcoord.xy);
	col.rgb += glow * glow * _Params.w;
#	endif
#ifdef EDGE_HIGHLIGHT
	half3 N = normalize(i.normal);
	half3 V = normalize(i.viewDir);
	half NoV = max(dot(N, V), 0);
	half edge = pow(1 - NoV, _EdgeParams.z);
#	ifdef HDR_ON
	col.rgb += edge * _EdgeTintColor.rgb * _EdgeParams.r;
#else
	col.rgb += edge * _EdgeTintColor.rgb * _EdgeParams.g;
#endif
#endif
#	ifdef USE_MASK
	col.a = tex2D(_MaskTex, i.texcoord.zw).r * i.color.a;
#	endif
#	ifdef HDR_ON
	col.rgb *= _Params.x;
#	else
	col.rgb *= _Params.y;
#	endif
#	ifdef DISSOLVE
	half alpha = 0;
	half param = -_Params.z;
	param = min(max(-1, param), 1);
	if (param < 1)
	{
		half dissolve = tex2D(_DissolveTex, i.texcoord.zw).r;
		alpha = saturate((dissolve - param) / (1 - param));
	}

	col.a *= alpha;//saturate(dissolve - _Params.z);
#	endif
#	if defined(MIX_ADD)
	col.rgb *= saturate(col.a + _Params.w);
#	elif defined(CLIP)
	clip(col.a - 0.5);
#	endif
#	ifndef POSTFX_ON
	col.rgb = sqrt(col.rgb);
#	endif
	return col;
}
