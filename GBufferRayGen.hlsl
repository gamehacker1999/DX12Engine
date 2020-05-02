#include "RTUtils.hlsli"

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0);

RWTexture2D< float4 > gDiffuse : register(u1);
RWTexture2D< float4 > gPosition : register(u2);
RWTexture2D< float4 > gNormal : register(u3);
RWTexture2D< float4 > gAlbedo : register(u4);


// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

struct RayTraceCameraData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
};


ConstantBuffer<RayTraceCameraData> cameraData: register(b0);

[shader("raygeneration")]
void GBufferRayGen() 
{
    // Initialize the ray payload
    GbufferPayload payload = { float3(0,0,0),float3(0,0,0) ,float3(0,0,0) ,float3(0,0,0) };

    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);

    uint rndseed = initRand(launchIndex.x, launchIndex.y);

    //creating the rayDescription
    RayDesc ray;
    ray.Origin = mul(float4(0, 0, 0, 1), cameraData.iView);
    float4 target = mul(float4(d.x, -d.y, 1, 1), cameraData.iProj);
    ray.Direction = mul(float4(target.xyz, 0), cameraData.iView);
    ray.TMin = 0.01;
    ray.TMax = 100000;
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, payload);


    gDiffuse[launchIndex] = float4(payload.diffuse, 1.f);
    gPosition[launchIndex] = float4(payload.position, 1.f);
    gNormal[launchIndex] = float4(payload.normal, 1.f);
    gAlbedo[launchIndex] = float4(payload.albedo, 1.f);

}
