#include "RayGenIncludes.hlsli"
#include "Common.hlsl"



struct RayTraceCameraData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
};


ConstantBuffer<RayTraceCameraData> cameraData: register(b0);
RWStructuredBuffer<uint> newSequences : register(u0, space1);

[shader("raygeneration")]
void GBufferRayGen() 
{
    // Initialize the ray payload
    GbufferPayload payload = { float4(0, 0, 0, 0),float3(0,0,0) ,float3(0,0,0) ,float3(0,0,0) };

    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);

    uint rndseed = newSequences[launchIndex.y * 1920 + launchIndex.x];

    //creating the rayDescription
    RayDesc ray;
    ray.Origin = mul(float4(0, 0, 0, 1), cameraData.iView);
    float4 target = mul(float4(d.x, -d.y, 1, 1), cameraData.iProj);
    ray.Direction = mul(float4(target.xyz, 0), cameraData.iView);
    ray.TMin = 0.01;
    ray.TMax = 100000;
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, payload);


    gRoughnessMetallic[launchIndex] = payload.roughnessMetallic;
    gPosition[launchIndex] = float4(payload.position, 1.f);
    gNormal[launchIndex] = float4(payload.normal, 1.f);
    gAlbedo[launchIndex] = float4(payload.albedo, 1.f);

}
