#include "Common.hlsl"


struct Vertex
{
    float3 Position;	    // The position of the vertex
    float3 Normal;
    float3 Tangent;
    float2 UV;
};

StructuredBuffer<Vertex> vertex : register(t0);
RaytracingAccelerationStructure SceneBVH : register(t1);

struct DirectionalLight
{
    float4 ambientColor;
    float4 diffuse;
    float4 specularity;
    float3 direction;
};

static const float PI = 3.14159265f;


cbuffer LightData: register(b0)
{
    DirectionalLight light1;
    float3 cameraPosition;
};

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID+1].Normal * barycentrics.y + vertex[vertID+2].Normal * barycentrics.z;

    float3 L = -light1.direction;

    float NdotL = saturate(dot(normal, L));

    payload.colorAndDistance = float4((light1.diffuse*NdotL).rgb, 1);
 // payload.colorAndDistance = float4(1, 1, 0, RayTCurrent());
}



[shader("closesthit")]
void PlaneClosestHit(inout HitInfo payload, Attributes attrib)
{
    /**/float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    //hardcoding the light position for now
    float3 lightPos = float3(-20, 20, 0);
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();    
    float3 lightDirection = normalize(lightPos- worldOrigin);

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    normal = normalize(normal);

    float3 rayDir = normalize(WorldRayDirection());

    HitInfo reflectedColor;

    reflectedColor.colorAndDistance = float4(0, 0, 0, 0);

    float3 reflectedDirection = reflect(rayDir, normal);

    /*RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = reflectedDirection;
    ray.TMin = 0.01;
    ray.TMax = 100000;*/

    /**/RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = lightDirection;
    ray.TMin = 0.01;
    ray.TMax = 100000;

    bool hit = true;

    //initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

    //trace the ray
    //TraceRay(SceneBVH, RAY_FLAG_NONE, ~0x02, 0, 2, 0, ray, reflectedColor);

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);


   /* uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;

    float3 L = -light1.direction;

    float NdotL = saturate(dot(normal, L));*/

    float factor = shadowPayload.isHit ? 0.3f:1.0f;

    //payload.colorAndDistance = reflectedColor.colorAndDistance;
    payload.colorAndDistance = float4(float3(0,1,0)*factor, RayTCurrent());
}
