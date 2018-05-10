half3 EnvBRDFApprox(half3 SpecularColor, half Roughness, half NoV)
{
	// [ Lazarov 2013, "Getting More Physical in Call of Duty: Black Ops II" ]
	// Adaptation to fit our G term.
	const half4 c0 = { -1, -0.0275, -0.572, 0.022 };
	const half4 c1 = { 1, 0.0425, 1.04, -0.04 };
	half4 r = Roughness * c0 + c1;
	half a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	half2 AB = half2(-1.04, 1.04) * a004 + r.zw;

	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	// Note: this is needed for the 'specular' show flag to work, since it uses a SpecularColor of 0
	AB.y *= saturate(50.0 * SpecularColor.g);

	return SpecularColor * AB.x + AB.y;
}

half EnvBRDFApproxNonmetal(half Roughness, half NoV)
{
	// Same as EnvBRDFApprox( 0.04, Roughness, NoV )
	const half2 c0 = { -1, -0.0275 };
	const half2 c1 = { 1, 0.0425 };
	half2 r = Roughness * c0 + c1;
	return min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
}

half PhongApprox(half Roughness, half RoL)
{
	half a = Roughness * Roughness;			// 1 mul
											//!! Ronin Hack?
	a = max(a, 0.008);						// avoid underflow in FP16, next sqr should be bigger than 6.1e-5
	half a2 = a * a;						// 1 mul
	half rcp_a2 = rcp(a2);					// 1 rcp
											//half rcp_a2 = exp2( -6.88886882 * Roughness + 6.88886882 );

											// Spherical Gaussian approximation: pow( x, n ) ~= exp( (n + 0.775) * (x - 1) )
											// Phong: n = 0.5 / a2 - 0.5
											// 0.5 / ln(2), 0.275 / ln(2)
	half c = 0.72134752 * rcp_a2 + 0.39674113;	// 1 mad
	half p = rcp_a2 * exp2(c * RoL - c);		// 2 mad, 1 exp2, 1 mul
												// Total 7 instr
	return min(p, rcp_a2);						// Avoid overflow/underflow on Mali GPUs
}

half D_GGX(half Roughness, half NoH)
{
	half a = Roughness * Roughness;
	half a2 = a * a;
	half d = (NoH * a2 - NoH) * NoH + 1;	// 2 mad
	return a2 / (PI*d*d);					// 4 mul, 1 rcp
}

half Vis_SmithJointApprox(half Roughness, half NoV, half NoL)
{
	half a = Roughness * Roughness;
	half Vis_SmithV = NoL * (NoV * (1 - a) + a);
	half Vis_SmithL = NoV * (NoL * (1 - a) + a);
	// Note: will generate NaNs with Roughness = 0.  MinRoughness is used to prevent this
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

half3 F_Schlick(half3 SpecularColor, half VoH)
{
	half Fc = Pow5(1 - VoH);					// 1 sub, 3 mul
												//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad

												// Anything less than 2% is physically impossible and is instead considered to be shadowing
	return saturate(50.0 * SpecularColor.g) * Fc + (1 - Fc) * SpecularColor;

}
