
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

Texture2D material[]: register(t0,space1);
SamplerState basicSampler: register(s0);

static const float PI = 3.14159265f;

struct Index
{
    uint index;
};

ConstantBuffer<Index> entityIndex: register(b1);


cbuffer LightData: register(b0)
{
    DirectionalLight light1;
    float3 cameraPosition;
};

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    payload.rayDepth++;



    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    normal = ConvertFromObjectToWorld(normal);
    float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;
    position = ConvertFromObjectToWorld(position);
    float3 tangent = vertex[vertID].Tangent * barycentrics.x + vertex[vertID + 1].Tangent * barycentrics.y + vertex[vertID + 2].Tangent * barycentrics.z;
    tangent = ConvertFromObjectToWorld(tangent);

    float2 uv = vertex[vertID].UV * barycentrics.x + vertex[vertID + 1].UV * barycentrics.y + vertex[vertID + 2].UV * barycentrics.z;

    uint index = entityIndex.index;
    float3 texColor = material[index + 0].SampleLevel(basicSampler, uv, 0).rgb;
    float3 normalColor = material[index + 1].SampleLevel(basicSampler, uv, 0).xyz;
    float3 unpackedNormal = normalColor * 2.0f - 1.0f;

    //orthonormalizing T, B and N using the gram-schimdt process
    float3 N = normalize(normal);
    float3 T = tangent - dot(tangent, N) * N;
    T = normalize(T);
    float3 Bi = normalize(cross(T, N));

    float3x3 TBN = float3x3(T, Bi, N); //getting the tbn matrix

    //transforming normal from map to world space
    float3 finalNormal = normalize(mul(unpackedNormal, TBN));

    //hardcoding the light position for now
    float3 lightPos = float3(-20, 20, 0);
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldOrigin);
    float3 L = -light1.direction;
    float NdotL = saturate(dot(finalNormal, L));

    /**/RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = L;
    ray.TMin = 1;
    ray.TMax = 100000;

    bool hit = true;

    //initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

    //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);


    //float factor = shadowPayload.isHit ? 0.3f : 1.0f;

    payload.color = (float4((light1.diffuse*NdotL).rgb* texColor, 1)).rgb / M_PI;

    payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(finalNormal);
    payload.diffuseColor = texColor;

   // if (payload.rayDepth <14)
   // {
   //     float3 giDir = getCosHemisphereSample(payload.rndseed, normal);
   //     float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
   //     
   //     HitInfo giPayload = { float3(0,0,0),payload.rayDepth,payload.rndseed,payload.currentPosition,payload.normal,float3(0,0,0) };
   // 
   //     /**/RayDesc ray;
   //     ray.Origin = position;
   //     ray.Direction = giDir;
   //     ray.TMin = 0.0001;
   //     ray.TMax = 100000;
   // 
   //     TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
   //     payload.color += float4(1,1,1,1)*giPayload.color;
   // }

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
void PlaneClosestHit(inout HitInfo payload, Attributes attrib)
{
    payload.rayDepth++;


    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    normal = ConvertFromObjectToWorld(normal);
    float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;
    position = ConvertFromObjectToWorld(position);
    float3 tangent = vertex[vertID].Tangent * barycentrics.x + vertex[vertID + 1].Tangent * barycentrics.y + vertex[vertID + 2].Tangent * barycentrics.z;
    tangent = ConvertFromObjectToWorld(tangent);
    float2 uv = vertex[vertID].UV * barycentrics.x + vertex[vertID + 1].UV * barycentrics.y + vertex[vertID + 2].UV * barycentrics.z;

    uint index = entityIndex.index;
    float3 texColor = material[index + 0].SampleLevel(basicSampler, uv, 0).rgb;
    float3 normalColor = material[index + 1].SampleLevel(basicSampler, uv, 0).xyz;
    float3 unpackedNormal = normalColor * 2.0f - 1.0f;

    //orthonormalizing T, B and N using the gram-schimdt process
    float3 N = normalize(normal);
    float3 T = tangent - dot(tangent, N) * N;
    T = normalize(T);
    float3 Bi = normalize(cross(T, N));

    float3x3 TBN = float3x3(T, Bi, N); //getting the tbn matrix

    //transforming normal from map to world space
    float3 finalNormal = normalize(mul(unpackedNormal, TBN));

    //hardcoding the light position for now
    float3 lightPos = float3(-20, 20, 0);
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldOrigin);
    float3 L = -light1.direction;
    float NdotL = saturate(dot(finalNormal, L));

    /**/RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = L;
    ray.TMin = 1;
    ray.TMax = 100000;

    bool hit = true;

    //initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

    //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);


    //float factor = shadowPayload.isHit ? 0.3f : 1.0f;

    payload.color = (float4((light1.diffuse * NdotL).rgb* texColor, 1)).rgb / M_PI;

    payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(finalNormal);
    payload.diffuseColor = texColor;

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

   // payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();


}
