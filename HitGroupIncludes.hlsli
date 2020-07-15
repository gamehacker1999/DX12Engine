#include "RTUtils.hlsli"

void GetPrimitiveProperties(inout float3 texColor, inout float3 position, inout float3 normal, inout float3 metalColor, inout float roughness, Attributes attrib)
{
	
    float3 barycentrics =
        float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
	
    uint vertID = PrimitiveIndex() * 3;
    normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
    normal = ConvertFromObjectToWorld(normal);
    position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;
    position = ConvertFromObjectToWorld(position);
    float3 tangent = vertex[vertID].Tangent * barycentrics.x + vertex[vertID + 1].Tangent * barycentrics.y + vertex[vertID + 2].Tangent * barycentrics.z;
    tangent = ConvertFromObjectToWorld(tangent);
    float2 uv = vertex[vertID].UV * barycentrics.x + vertex[vertID + 1].UV * barycentrics.y + vertex[vertID + 2].UV * barycentrics.z;

    uint index = entityIndex.index;
    texColor = material[index + 0].SampleLevel(basicSampler, uv, 0).rgb;
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
	
    normal = finalNormal;

    //getting the metalness of the pixel
    metalColor = material[index + 3].SampleLevel(basicSampler, uv, 0).xyz;

    float3 f0 = float3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, texColor.xyz, metalColor);

    //getting the roughness of pixel
    roughness = material[index + 2].SampleLevel(basicSampler, uv, 0).x;
}

