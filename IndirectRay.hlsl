#include "Common.hlsl"

#include"RTUtils.hlsli"

struct IndirectPayload
{
    float3 color;
    float rndseed;
};

TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

[shader("miss")]
void Miss(inout IndirectPayload payload : SV_RayPayload)
{
    float3 color = skyboxTexture.SampleLevel(basicSampler, WorldRayDirection(), 0).rgb;
    payload.colorAndDistance = color;
}

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

float3 DiffuseShade(float3 pos, float3 normal, float3 diffuseColor, float rndseed)
{

}

[shader("closesthit")]
void IndirectClosestHit(inout IndirectPayload payload, Attributes attrib)
{
    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    uint vertID = PrimitiveIndex() * 3;
    float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;

    float3 L = -light1.direction;

    float NdotL = saturate(dot(normal, L));

    float3 diffuseColor = float3(1,0,0);

    payload.color = DiffuseShade(position,normal,diffuseColor,payload.rndseed);
    // payload.colorAndDistance = float4(1, 1, 0, RayTCurrent());
}

float3 DiffuseShade(float3 pos, float3 norm, float3 difColor, inout uint rndseed)
{
    float3 L = -light1.direction;

    float NdotL = saturate(dot(norm, L));

    bool isLit = ShootShadowRay(pos, L, 1.0e-4, 1.0e+38);

    float factor = isLit ? 0.3f : 1.0f;

    float3 rayColor = factor * light1.diffuse.rgb;

    return NdotL * rayColor * (diffuseColor / PI);
}
