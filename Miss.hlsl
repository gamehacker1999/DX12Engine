#include "Common.hlsl"
#include "Utils.hlsli"

Texture2D skyboxTexture: register(t0);
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
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.rayDepth++;
	float2 uv = DirectionToLatLongUV(WorldRayDirection());
    float3 color = skyboxTexture.SampleLevel(basicSampler, uv, 0).rgb;
    
    //if(color.x>=1 && color.y>=1 && color.z>=1)
    //{
    //    color = float3(0, 0, 0);
    //
    //}
    
    color = pow(color, 2.2);
    
    payload.currentPosition = WorldRayOrigin() + RayTCurrent()* WorldRayDirection();
    payload.color = color;
    payload.diffuseColor = color;
    //payload.currentPosition = float3(0, 0, 0);
    payload.normal = -WorldRayDirection();
    payload.normal = normalize(payload.normal);
}