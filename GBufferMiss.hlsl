#include "Common.hlsl"

TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

[shader("miss")]
void GBufferMiss(inout GbufferPayload payload : SV_RayPayload)
{
    float3 color = skyboxTexture.SampleLevel(basicSampler, WorldRayDirection(), 0).rgb;
    payload.normal = float3(0, 0, 0);
    payload.albedo = color;
    payload.diffuse = float3(0.f, 0.f, 0.f);
    payload.position = float3(0, 0, 0);
}