#include "Common.hlsl"

TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.rayDepth++;
    float3 color = skyboxTexture.SampleLevel(basicSampler, WorldRayDirection(),0).rgb;
    payload.color = color;
    payload.diffuseColor = color;
    payload.currentPosition = float3(0, 0, 0);
    payload.normal = float3(0, 0, 0);
}