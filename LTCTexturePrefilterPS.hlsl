
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D ltcTexture : register(t0);
SamplerState basicSampler : register(s0);

cbuffer ltcPrefilterExternData : register(b0)
{
    float width;
    float height;
    uint mipLevel;
};

static const float BlurWeights[13] =
{
   0.002216,
   0.008764,
   0.026995,
   0.064759,
   0.120985,
   0.176033,
   0.199471,
   0.176033,
   0.120985,
   0.064759,
   0.026995,
   0.008764,
   0.002216,
};

float4 main(VertexToPixel input) : SV_TARGET
{
    float3 color = ltcTexture.Sample(basicSampler, input.uv).rgb;
    
    float sampleRadius = 1 << mipLevel;
    
    for (int i = -sampleRadius; i < sampleRadius;i++)
    {
        for (int j = -sampleRadius; j < sampleRadius;j++)
        {
            float2 offset = float2(i, j) / float2(width, height);
            float2 offsettedUV = input.uv + offset;
            float3 sampledCol = ltcTexture.Sample(basicSampler, offsettedUV).rgb;
            color += sampledCol;
        }

    }
    
    color /= sampleRadius * sampleRadius;
    
    return (1, 0, 0, 1);
}