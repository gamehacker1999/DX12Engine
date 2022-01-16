#include "RayGenIncludes.hlsli"
#include "Common.hlsl"

ConstantBuffer<RayTraceExternData> cameraData : register(b0);

[shader("raygeneration")]
void RayGenTransparency()
{
    // Initialize the ray payload
    HitInfo payload;
    payload.color = float3(0, 0, 0);
    payload.rayDepth = 0.0;
    payload.normal = float3(0, 0, 0);

    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);

    //creating the rayDescription
    RayDesc ray;
    ray.Origin = mul(float4(0, 0, 0, 1), cameraData.iView);
    float4 target = mul(float4(d.x, -d.y, 1, 1), cameraData.iProj);
    ray.Direction = mul(float4(target.xyz, 0), cameraData.iView);
    ray.TMin = 0.01;
    ray.TMax = 100000;
    TraceRay(SceneBVH, RAY_FLAG_NONE, RAYTRACING_INSTANCE_TRANSCLUCENT, 0, 2, 0, ray, payload);

    gOutputTransparency[launchIndex] = float4(payload.color, 1.0);

}