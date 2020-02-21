#include "Common.hlsl"

struct Vertex
{
    float3 Position;	    // The position of the vertex
    float3 Normal;
    float3 Tangent;
    float2 UV;
};

StructuredBuffer<Vertex> vertex : register(t0);

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

    payload.colorAndDistance = float4((light1.diffuse*NdotL).rgb, RayTCurrent());
 // payload.colorAndDistance = float4(1, 1, 0, RayTCurrent());
}
