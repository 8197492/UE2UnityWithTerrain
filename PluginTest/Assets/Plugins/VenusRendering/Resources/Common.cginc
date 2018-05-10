#define PI (3.141592653589793)

float Pow5(float x)
{
	float xx = x * x;
	return xx * xx * x;
}

float2 Pow5(float2 x)
{
	float2 xx = x * x;
	return xx * xx * x;
}

float3 Pow5(float3 x)
{
	float3 xx = x * x;
	return xx * xx * x;
}

float4 Pow5(float4 x)
{
	float4 xx = x * x;
	return xx * xx * x;
}

#if defined(USE_PBR) || defined(USE_DISSOLVE)
#define USE_WORLD_POS
#endif

#if defined(USE_PBR) || defined(USE_FOCUS)
#define USE_NOV
#endif

#if defined(USE_PLANAR) && defined(PLANAR_ON)
#define USE_SCREEN_UV
#endif

#if defined(SUNLIGHT_ON)
half3 _sunLightDirection;
half4 _sunLightColor;
half2 _sunLightPenumbra;
#endif

half3 normal_map(in half3 normalColor, in half3x3 TtoW)
{
	normalColor = 2.0f * (normalColor - 0.5f);
	return normalize(mul(normalColor, TtoW));
}

half3 main_light_dir()
{
#if defined(SUNLIGHT_ON)
	return _sunLightDirection;
#else
	return half3(0, 1, 0);
#endif
}

half3 main_light_color()
{
#if defined(SUNLIGHT_ON)
	half3 color = _sunLightColor.rgb;
#	if !defined(HDR_ON)
	color *= _sunLightColor.a;
#	endif
	return color;
#else
	return half3(0, 0, 0);
#endif
}

half GetLuminance(half3 LinearColor)
{
	return dot(LinearColor, half3(0.3, 0.59, 0.11));
}

#ifndef ENV_MIP_NUM
#	define ENV_MIP_NUM (7)
#endif

half ComputeReflectionCaptureMipFromRoughness(half Roughness)
{
	// Heuristic that maps roughness to mip level
	// This is done in a way such that a certain mip level will always have the same roughness, regardless of how many mips are in the texture
	// Using more mips in the cubemap just allows sharper reflections to be supported
	half LevelFrom1x1 = 1 - 1.2 * log2(Roughness);
	return ENV_MIP_NUM - 1 - LevelFrom1x1;
}

half3 RGBMDecode(fixed4 rgbm)
{
	return rgbm.rgb * (rgbm.a * 16.0f);
}

half3 GetSphereCaptureVector(half3 ReflectionVector, half3 WorldPosition, half4 CapturePositionAndRadius)
{
	half3 ProjectedCaptureVector = ReflectionVector;

	half3 RayDirection = ReflectionVector;
	half ProjectionSphereRadius = CapturePositionAndRadius.w * 1.2f;
	half SphereRadiusSquared = ProjectionSphereRadius * ProjectionSphereRadius;

	half3 ReceiverToSphereCenter = WorldPosition - CapturePositionAndRadius.xyz;
	half ReceiverToSphereCenterSq = dot(ReceiverToSphereCenter, ReceiverToSphereCenter);

	half3 CaptureVector = WorldPosition - CapturePositionAndRadius.xyz;
	half CaptureVectorLength = sqrt(dot(CaptureVector, CaptureVector));
	half NormalizedDistanceToCapture = saturate(CaptureVectorLength / CapturePositionAndRadius.w);

	// Find the intersection between the ray along the reflection vector and the capture's sphere
	half3 QuadraticCoef;
	QuadraticCoef.x = 1;
	QuadraticCoef.y = 2 * dot(RayDirection, ReceiverToSphereCenter);
	QuadraticCoef.z = ReceiverToSphereCenterSq - SphereRadiusSquared;

	half Determinant = QuadraticCoef.y * QuadraticCoef.y - 4 * QuadraticCoef.z;
	UNITY_BRANCH
		if (Determinant >= 0)
		{
			half FarIntersection = (sqrt(Determinant) - QuadraticCoef.y) * 0.5;

			half3 IntersectPosition = WorldPosition + FarIntersection * RayDirection;
			ProjectedCaptureVector = IntersectPosition - CapturePositionAndRadius.xyz;
		}
	return ProjectedCaptureVector;
}
