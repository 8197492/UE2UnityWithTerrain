float4 _FogParams;
float4 _FogColorParams;
float4 _FogParams3;

half4 GetExponentialHeightFog(float3 WorldPositionRelativeToCamera)
{
	const float FLT_EPSILON2 = 0.01f;
	const half MinFogOpacity = _FogColorParams.w;
	WorldPositionRelativeToCamera *= 100;
	float3 CameraToReceiver = WorldPositionRelativeToCamera;
	float CameraToReceiverLengthSqr = dot(CameraToReceiver, CameraToReceiver);
	float CameraToReceiverLengthInv = rsqrt(CameraToReceiverLengthSqr);
	float CameraToReceiverLength = CameraToReceiverLengthSqr * CameraToReceiverLengthInv;
	half3 CameraToReceiverNormalized = CameraToReceiver * CameraToReceiverLengthInv;

	float RayOriginTerms = _FogParams.x;
	float RayLength = CameraToReceiverLength;
	float RayDirectionZ = CameraToReceiver.y;

	float ExcludeDistance = _FogParams.w;
	if (ExcludeDistance > 0)
	{
		float ExcludeIntersectionTime = ExcludeDistance * CameraToReceiverLengthInv;
		float CameraToExclusionIntersectionZ = ExcludeIntersectionTime * CameraToReceiver.y;
		float ExclusionIntersectionZ = _WorldSpaceCameraPos.y * 100.0f + CameraToExclusionIntersectionZ;
		float ExclusionIntersectionToReceiverZ = CameraToReceiver.y - CameraToExclusionIntersectionZ;

		// Calculate fog off of the ray starting from the exclusion distance, instead of starting from the camera
		RayLength = (1.0f - ExcludeIntersectionTime) * CameraToReceiverLength;
		RayDirectionZ = ExclusionIntersectionToReceiverZ;

		float Exponent = max(-127.0f, _FogParams.y * (ExclusionIntersectionZ - _FogParams3.y));
		RayOriginTerms = _FogParams3.x * exp2(-Exponent);
	}

	float EffectiveZ = (abs(RayDirectionZ) > FLT_EPSILON2) ? RayDirectionZ : FLT_EPSILON2;
	float Falloff = max(-127.0f, _FogParams.y * EffectiveZ);	// if it's lower than -127.0, then exp2() goes crazy in OpenGL's GLSL.
	float ExponentialHeightLineIntegralShared = RayOriginTerms * (1.0f - exp2(-Falloff)) / Falloff;
	float ExponentialHeightLineIntegral = ExponentialHeightLineIntegralShared * RayLength;

	half ExpFogFactor = max(saturate(exp2(-ExponentialHeightLineIntegral)), MinFogOpacity);
	half3 InscatteringColor = _FogColorParams.rgb;

	return half4((InscatteringColor) * (1 - ExpFogFactor), ExpFogFactor);
}
