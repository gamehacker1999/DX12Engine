
#include "RTUtils.hlsli"

struct Vertex
{
    float3 Position;	    // The position of the vertex
    float3 Normal;
    float3 Tangent;
    float2 UV;
};

StructuredBuffer<Vertex> vertex : register(t0);
RaytracingAccelerationStructure SceneBVH : register(t1);

Texture2D material[]: register(t0, space1);
SamplerState basicSampler: register(s0);


static const float PI = 3.14159265f;


cbuffer LightData: register(b0)
{
    DirectionalLight light1;
    float3 cameraPosition;
};

[shader("closesthit")]
void GBufferClosestHit(inout GbufferPayload payload, Attributes attrib)
{
    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;

    //normal = normalize(normal);

    //hardcoding the light position for now
    float3 lightPos = float3(-20, 20, 0);
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldOrigin);


    float3 L = -light1.direction;

    float NdotL = saturate(dot(normal, L));

    payload.diffuse = NdotL * light1.diffuse.rgb* float3(1, 1, 1);
    payload.albedo = float3(1, 1, 1);
    payload.position = position;
    payload.normal = normalize(normal);

    ///**/RayDesc ray;
    //ray.Origin = worldOrigin;
    //ray.Direction = L;
    //ray.TMin = 1;
    //ray.TMax = 100000;
    //
    //bool hit = true;
    //
    ////initialize the hit payload
    //ShadowHitInfo shadowPayload;
    //shadowPayload.isHit = false;
    //
    //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);
    //
    //
    //float factor = shadowPayload.isHit ? 0.3f : 1.0f;
    //
    //payload.color = (float4((light1.diffuse * NdotL).rgb, 1) * float4(1, 1, 1, 1) * factor).rgb;
    //
    //payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    //payload.normal = normalize(normal);
    //payload.diffuseColor = float3(1, 0, 0);
    //
    //if (payload.rayDepth < 2)
    //{
    //    float3 giDir = getCosHemisphereSample(payload.rndseed, normal);
    //    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    //
    //    HitInfo giPayload = { float3(0,0,0),payload.rayDepth,payload.rndseed,payload.currentPosition,payload.normal,float3(0,0,0) };
    //
    //    /**/RayDesc ray;
    //    ray.Origin = position;
    //    ray.Direction = normal;
    //    ray.TMin = 0.0001;
    //    ray.TMax = 100000;
    //
    //    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
    //    payload.color += float4(1, 1, 1, 1) * giPayload.color;
    //}

    //for (int i = 0; i < 64; i++)
    //{
    //    if (payload.rayDepth < 2)
    //    {
    //        float3 giDir = getCosHemisphereSample(payload.rndseed, normal);
    //        float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    //
    //        HitInfo giPayload = { float3(0,0,0),payload.rayDepth,payload.rndseed,payload.currentPosition,payload.normal,float3(0,0,0) };
    //
    //        /**/RayDesc ray;
    //        ray.Origin = position;
    //        ray.Direction = giDir;
    //        ray.TMin = 0.0001;
    //        ray.TMax = 100000;
    //
    //        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
    //        payload.color += float4(1, 0, 0, 1) * giPayload.color;
    //    }
    //}

   // payload.color;
 // payload.colorAndDistance = float4(1, 1, 0, RayTCurrent());
}



[shader("closesthit")]
void GBufferPlaneClosestHit(inout GbufferPayload payload, Attributes attrib)
{

    /**/float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    //hardcoding the light position for now
    float3 lightPos = float3(-20, 20, 0);
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldOrigin);

    uint vertID = PrimitiveIndex() * 3;
    float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    normal = normalize(normal);

    float3 L = -light1.direction;

    float NdotL = saturate(dot(normal, L));

    payload.normal = normal;

    float3 rayDir = normalize(WorldRayDirection());

    HitInfo reflectedColor;

    //reflectedColor.color = payload.color;

    //float3 reflectedDirection = reflect(rayDir, normal);

    payload.diffuse = NdotL * light1.diffuse.rgb*float3(0,1,0);
    payload.albedo = float3(0, 1, 0);
    payload.position = position;
    payload.normal = normalize(normal);

    /*RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = reflectedDirection;
    ray.TMin = 0.01;
    ray.TMax = 100000;*/

   /**/RayDesc ray;
   ray.Origin = worldOrigin;
   ray.Direction = L;
   ray.TMin = 0.01;
   ray.TMax = 100000;
    //
    //payload.currentPosition = worldOrigin;
    //payload.diffuseColor = float3(0, 1, 0);
    //
    //bool hit = true;
    //
    ////initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;
    //
    ////trace the ray
    //
    ////if (payload.rayDepth < 2)
    ////{
    ////    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, reflectedColor);
    ////}
    //
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);
    //
    //
    //
    //
    ///* uint vertID = PrimitiveIndex() * 3;
    // float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    //
    // float3 L = -light1.direction;
    //
    // float NdotL = saturate(dot(normal, L));*/
    //
    float factor = shadowPayload.isHit ? 0.3f : 1.0f;
    //
    ////payload.colorAndDistance = reflectedColor.colorAndDistance;
    //
    //
    payload.diffuse = (float3((light1.diffuse * NdotL * factor).rgb) * float3(0, 1, 0)).rgb;
    //
    //if (payload.rayDepth < 2)
    //{
    //    float3 giDir = getCosHemisphereSample(payload.rndseed, normal);
    //    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    //
    //    HitInfo giPayload = { float3(0,0,0),payload.rayDepth,payload.rndseed,float3(0,0,0),float3(0,0,0),float3(0,0,0) };
    //
    //    /**/RayDesc ray;
    //    ray.Origin = worldOrigin;
    //    ray.Direction = giDir;
    //    ray.TMin = 0.01;
    //    ray.TMax = 100000;
    //
    //    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
    //    payload.color += (float4(0, 1, 0, 1) * giPayload.color).rgb;
    //}
    //
    //// payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();


}
