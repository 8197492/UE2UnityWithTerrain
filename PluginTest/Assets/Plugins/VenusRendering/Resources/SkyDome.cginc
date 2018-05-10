#include "UnityCG.cginc"
#include "Lighting.cginc"

half _SunSize;
half _Exposure;
half _AtmosphereThickness;

half3 _SkyTint;
half3 _SkyColor;
half4 _FogParams;
half4 _CloudParams;

half3 _sunLightDirection;
half4 _sunLightColor;
half4 _FogColorParams;

static const float3 kDefaultScatteringWavelength = float3(.65, .57, .475);
static const float3 kVariableRangeForScatteringWavelength = float3(.15, .15, .15);

#define OUTER_RADIUS 1.025
static const float kOuterRadius = OUTER_RADIUS;
static const float kOuterRadius2 = OUTER_RADIUS*OUTER_RADIUS;
static const float kInnerRadius = 1.0;
static const float kInnerRadius2 = 1.0;

static const float kCameraHeight = 0.0001;

#define kRAYLEIGH (lerp(0.0, 0.0025, pow(_AtmosphereThickness,2.5)))
#define kMIE 0.0010
#define kSUN_BRIGHTNESS 20.0

#define kMAX_SCATTER 50.0
#define kPI (3.14159265)

static const half kSunScale = 400.0 * kSUN_BRIGHTNESS;
static const float kKmESun = kMIE * kSUN_BRIGHTNESS;
static const float kKm4PI = kMIE * 4.0 * kPI;
static const float kScale = 1.0 / (OUTER_RADIUS - 1.0);
static const float kScaleDepth = 0.25;
static const float kScaleOverScaleDepth = (1.0 / (OUTER_RADIUS - 1.0)) / 0.25;
static const float kSamples = 50;

#define MIE_G (-0.990)
#define MIE_G2 0.9801

#define SKY_GROUND_THRESHOLD 0.02

half getRayleighPhase(half eyeCos2)
{
	return 0.75 + 0.75*eyeCos2;
}
half getRayleighPhase(half3 light, half3 ray)
{
	half eyeCos = dot(light, ray);
	return getRayleighPhase(eyeCos * eyeCos);
}

struct appdata_t
{
	float4 vertex : POSITION;
};

struct v2f
{
	float4	pos				: SV_POSITION;
	float3	vertex			: TEXCOORD0;
	half3	skyColor		: TEXCOORD1;
	half3	sunColor		: TEXCOORD2;	
};


float scale(float inCos)
{
	float x = 1.0 - inCos;
	return 0.25 * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));
}

v2f vert(appdata_t v)
{
	v2f OUT;
	OUT.pos = UnityObjectToClipPos(v.vertex);

	float3 kSkyTintInGammaSpace = _SkyTint;
	float3 kScatteringWavelength = lerp(
		kDefaultScatteringWavelength - kVariableRangeForScatteringWavelength,
		kDefaultScatteringWavelength + kVariableRangeForScatteringWavelength,
		half3(1, 1, 1) - kSkyTintInGammaSpace);
	float3 kInvWavelength = 1.0 / pow(kScatteringWavelength, 4);

	float kKrESun = kRAYLEIGH * kSUN_BRIGHTNESS;
	float kKr4PI = kRAYLEIGH * 4.0 * kPI;

	float3 cameraPos = float3(0, kInnerRadius + kCameraHeight, 0);

	float3 eyeRay = normalize(mul((float3x3)unity_ObjectToWorld, v.vertex.xyz));

	float far = 0.0;
	half3 cIn, cOut;

	if (eyeRay.y < -0) eyeRay.y = -eyeRay.y;

	far = sqrt(kOuterRadius2 + kInnerRadius2 * eyeRay.y * eyeRay.y - kInnerRadius2) - kInnerRadius * eyeRay.y;

	float3 pos = cameraPos + far * eyeRay;

	float height = kInnerRadius + kCameraHeight;
	float depth = exp(kScaleOverScaleDepth * (-kCameraHeight));
	float startAngle = dot(eyeRay, cameraPos) / height;
	float startOffset = depth*scale(startAngle);

	float sampleLength = far / 5;
	float scaledLength = sampleLength * kScale;
	float3 sampleRay = eyeRay * sampleLength;
	float3 samplePoint = cameraPos + sampleRay * 0.5;

	float3 frontColor = float3(0.0, 0.0, 0.0);
	for(int i=0; i<int(kSamples); i++)
	{
		float height = length(samplePoint);
		float depth = exp(kScaleOverScaleDepth * (kInnerRadius - height));
		float lightAngle = dot(_sunLightDirection.xyz, samplePoint) / height;
		float cameraAngle = dot(eyeRay, samplePoint) / height;
		float scatter = (startOffset + depth*(scale(lightAngle) - scale(cameraAngle)));
		float3 attenuate = exp(-clamp(scatter, 0.0, kMAX_SCATTER) * (kInvWavelength * kKr4PI + kKm4PI));

		frontColor += attenuate * (depth * scaledLength);
		samplePoint += sampleRay;
	}
	frontColor *= _SkyColor;
	cIn = frontColor * (kInvWavelength * kKrESun);
	cOut = frontColor * kKmESun;

	OUT.vertex = -v.vertex;
	OUT.skyColor = _Exposure * (cIn * getRayleighPhase(_sunLightDirection.xyz, -eyeRay));
	OUT.sunColor = _Exposure * (cOut * _sunLightColor.xyz);

	return OUT;
}

half getMiePhase(half eyeCos, half eyeCos2)
{
	half temp = 1.0 + MIE_G2 - 2.0 * MIE_G * eyeCos;
	temp = pow(temp, pow(_SunSize, 0.65) * 10);
	temp = max(temp, 1.0e-4);
	temp = 1.5 * ((1.0 - MIE_G2) / (2.0 + MIE_G2)) * (1.0 + eyeCos2) / temp;

	return temp;
}

half calcSunSpot(half3 vec1, half3 vec2)
{
	half3 delta = vec1 - vec2;
	half dist = length(delta);
	half spot = 1.0 - smoothstep(0.0, _SunSize, dist);
	return kSunScale * spot * spot;
}

sampler2D _CloudTex;

half4 frag(v2f i) : SV_Target
{
	half3 color = half3(0.0, 0.0, 0.0);
	float3 ray = normalize(mul((float3x3)unity_ObjectToWorld, i.vertex));
	color = i.skyColor;
	half eyeCos = dot(_sunLightDirection.xyz, ray);
	half eyeCos2 = eyeCos * eyeCos;
	half mie = getMiePhase(eyeCos, eyeCos2);
	color += mie * i.sunColor;
	color *= color;
	
	float2 cloudUV;
	float2 rayLand = normalize(-ray.xz);
	cloudUV.x = acos(rayLand.x);
	if (rayLand.y > 0)
	{
		cloudUV.x = 2 * kPI - cloudUV.x;
	}
	cloudUV.x *= _CloudParams.z / (2 * kPI);
	cloudUV.x = frac(_Time * _CloudParams.w + cloudUV.x);
	cloudUV.y = -smoothstep(_CloudParams.y, _CloudParams.x, acos(-ray.y) / kPI);
	half4 cloud = tex2D(_CloudTex, cloudUV);
	color = lerp(color, cloud.rgb, cloud.a);
#ifdef FOG_ON
	half fog = pow(smoothstep(_FogParams.x, _FogParams.y, -ray.y), _FogParams.z);
	color = lerp(color, _FogColorParams.rgb, fog);
#endif

#ifndef POSTFX_ON
	color = sqrt(color);
#endif

	return half4(color, 1000.0);
}
