#include "SphericalGaussian.hlsli"

//input normal map
Texture2D normalMap : register(t0);
Texture2D roughnessMap : register(t1);

RWTexture2D<float4> outputVMFMap : register(u0);
RWTexture2D<float4> outputRoughnessMaps : register(u1);

cbuffer externalData : register(b0)
{
    float2 outputSize;
    float2 textureSize;
    uint mipLevel;
};

float3 FetchNormal(uint2 samplePos)
{
    float3 normal = normalize(normalMap[samplePos].xyz * 2.0f - 1.0f);

    return normal;
}

void SolveVMF(float2 samplePosition, float sampleRadius, inout VMF vmfDist, inout float vmfRoughness)
{
    
    if (mipLevel == 0)
    {
        vmfDist.mu = FetchNormal(uint2(samplePosition)).xyz,
        vmfDist.alpha = 1.0f;
        vmfDist.kappa = 10000.0f;
        float roughness = roughnessMap[samplePosition].r;
        vmfRoughness = roughness;

    }
    
    else
    {
        float3 avgNormal = 0.0f;

        float2 topLeft = (-float(sampleRadius) / 2.0f) + 0.5f;

        for (uint y = 0; y < sampleRadius; ++y)
        {
            for (uint x = 0; x < sampleRadius; ++x)
            {
                float2 offset = topLeft + float2(x, y);
                float2 samplePos = floor(samplePosition + offset) + 0.5f;
                float3 sampleNormal = FetchNormal(samplePos);

                avgNormal += sampleNormal;
            }
        }

        avgNormal /= (sampleRadius * sampleRadius);

        float r = length(avgNormal);
        float kappa = 10000.0f;
        if (r < 1.0f)
            kappa = (3 * r - r * r * r) / (1 - r * r);

        float3 mu = normalize(avgNormal);

        vmfDist.mu = mu;
        vmfDist.alpha = 1.0f;
        vmfDist.kappa = kappa;

        // Pre-compute roughness map values
        float roughness = roughnessMap[samplePosition].r;
        vmfRoughness = sqrt(roughness * roughness + (2.0f / kappa));
    }
}

#define GROUP_SIZE 16

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{
    VMF vmfDist;
    
    uint2 outputPos = groupID.xy * uint2(GROUP_SIZE, GROUP_SIZE) + groupThreadID.xy;
    
    if (outputPos.x < outputSize.x && outputPos.y < outputSize.y)
    {
        float2 uv = (outputPos + 0.5) / outputSize;
        float sampleRadius = 1 << mipLevel;
        float2 samplePosition = uv * textureSize;
        
        float vmfRoughness;
        SolveVMF(samplePosition, sampleRadius, vmfDist, vmfRoughness);
        
        outputVMFMap[outputPos] = float4(vmfDist.mu.x, vmfDist.mu.y, 1.0f, 1.0 / vmfDist.kappa);
        outputRoughnessMaps[outputPos] = (float4(vmfRoughness, vmfRoughness, vmfRoughness, vmfRoughness));

    }

}