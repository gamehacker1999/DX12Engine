

//input normal map
Texture2D LTCTexture : register(t0);

RWTexture2D<uint4> outputLTCTex : register(u0);

cbuffer externalData : register(b0)
{
    float2 outputSize;
    float2 textureSize;
    uint mipLevel;
};

static const float offset[5] = { 0.0, 1.0, 2.0, 3.0, 4.0 };
static const float weight[5] = {0.2270270270, 0.1945945946, 0.1216216216,
                                  0.0540540541, 0.0162162162};

float3 PrefilterTexture(float2 samplePosition, float sampleRadius)
{
    float weight = 0.0f;
    float3 color = float3(0, 0, 0);
    
    for (int i = -sampleRadius / 2; i < sampleRadius / 2;i++)
    {
        for (int j = -sampleRadius / 2; j < sampleRadius / 2; j++)
        {
            float2 offset = float2(i, j);
            color += LTCTexture[samplePosition + offset].xyz;
        }

    }
    
    color /= (sampleRadius * sampleRadius);
    
    return color;
    
}

#define GROUP_SIZE 16

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{    
    uint2 outputPos = groupID.xy * uint2(GROUP_SIZE, GROUP_SIZE) + groupThreadID.xy;
    
    if (outputPos.x < outputSize.x && outputPos.y < outputSize.y)
    {
        float2 uv = (outputPos + 0.5) / outputSize;
        float sampleRadius = 1 << mipLevel;
        float2 samplePosition = uv * textureSize;

        outputLTCTex[outputPos] = float4(PrefilterTexture(samplePosition, sampleRadius), 1.0f);
    }

}