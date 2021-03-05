#include "RayGenIncludes.hlsli"


RWStructuredBuffer<uint> newSequences : register(u0, space1);


struct RayTraceCameraData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
};

ConstantBuffer<RayTraceCameraData> cameraData : register(b0);

[shader("raygeneration")]
void IndirectDiffuseRayGen()
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


    float3 pos = gPosition[launchIndex].xyz;
    float3 norm = gNormal[launchIndex].xyz;
    float3 albedo = gAlbedo[launchIndex].xyz;
    float3 metalColor = gRoughnessMetallic[launchIndex].rgb;
    float roughness = gRoughnessMetallic[launchIndex].a;
    
    float3 f0 = float3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, albedo, metalColor);
   
    float3 color = float3(0, 0, 0);

    if (norm.x == 0 && norm.y == 0 && norm.z == 0)
    {
        color = float3(0,0,0);
    }
    else
    {
        uint rndseed = newSequences[launchIndex.x * 1080 + launchIndex.y].r;

        float3 V = normalize(cameraPosition - pos);

        float3 indirectLight = float3(0, 0, 0);
        
        for (int i = 0; i < 1; i++)
        {
            // Do indirect lighting for global illumination
            indirectLight += IndirectDiffuseLighting(rndseed, pos, norm, V, metalColor.r,
	        albedo, f0, roughness, 0);
        }
        

        
        indirectLight /= 1;
        color += indirectLight;

    }

    gIndirectDiffuseOutput[launchIndex] = float4(color, 1.f);
    
}
