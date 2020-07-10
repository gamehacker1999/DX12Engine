#include "Common.hlsl"

TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

float3 CIEClearSky(in float3 dir, in float3 sunDir)
{
	float cosThetaV = dir.z;

	if (cosThetaV < 0.0f)
		cosThetaV = 0.0f;

	float cosThetaS = sunDir.z;
	float thetaS = acos(cosThetaS);

	float cosGamma = dot(sunDir, dir);
	float gamma = acos(cosGamma);

	float top1 = 0.91f + 10 * exp(-3 * gamma) + 0.45f * (cosGamma * cosGamma);
	float bot1 = 0.91f + 10 * exp(-3 * thetaS) + 0.45f * (cosThetaS * cosThetaS);

	float top2 = 1 - exp(-0.32f / (cosThetaV + 1e-6f));
	float bot2 = 1 - exp(-0.32f);

	return float3(1, 1, 1) * (top1 * top2) / (bot1 * bot2);

    // Draw a circle for the sun
    
    //if (true)
    //{
    //    float sunGamma = AngleBetween(dir, sunDir);
    //    color = lerp(SunColor, SkyColor, saturate(abs(sunGamma) / SunWidth));
    //}
	//
    //return max(color * lum, 0);
}


[shader("miss")]
void GBufferMiss(inout GbufferPayload payload : SV_RayPayload)
{
	float3 color = skyboxTexture.SampleLevel(basicSampler, WorldRayDirection(), 0).rgb;
    payload.normal = float3(0, 0, 0);
    payload.albedo = color;
    //payload.roughnessMetallic = float3(0.f, 0.f, 0.f);
    payload.position = float3(0, 0, 0);
}