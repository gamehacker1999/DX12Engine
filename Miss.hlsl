#include "Common.hlsl"

TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    float3 color = skyboxTexture.SampleLevel(basicSampler, WorldRayDirection(),0).rgb;
    payload.colorAndDistance = float4(color, 1.f);
}