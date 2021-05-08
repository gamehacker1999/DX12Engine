Texture2D mainColor : register(t0);
Texture2D depthTex : register(t1);

SamplerState pointSampler : register(s0);

cbuffer ExternalData
{
    uint width;
    uint height;
    uint oneOverWidth;
    uint oneOverHeight;
};

#include "Common.hlsl"
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

#define KERNEL_RADIUS 8

float Gaussian(float sigma, float x)
{
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

float4 BlurFunction(float2 uv, float r, float4 centerCol, float centerDepth, inout float totalWeight)
{
    float4 color = mainColor.Sample(pointSampler, uv);
    float depth = depthTex.Sample(pointSampler, uv).x;
    
    float sigma = KERNEL_RADIUS*0.5;
    
    float blurFalloff = 1.f / (2.f * sigma * sigma);
    
    float difference = (depth - centerDepth);
    float w = exp2(-r * r * blurFalloff - difference * difference);
    
    totalWeight += w;
    
    return color * w;
}

const float2 offsets[4] =
{
    float2(0., 0.),
    float2(0., 1.),
    float2(1., 0.),
    float2(1., 1.)
};

float4 main(VertexToPixel input) : SV_TARGET
{
    float4 centerCol = mainColor.Sample(pointSampler, input.uv);
    
    return centerCol;
    float centerDepth = depthTex.Sample(pointSampler, input.uv).r;
    
    float4 finalCol = centerCol;
    float weight = 1;
    
    //applying the bilateral blur
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++)
    {
        float2 curUV = input.uv + i * float2(1.f / float(width), 0);
        finalCol += BlurFunction(curUV, i, centerCol, centerDepth, weight);
    }
    
    //applying the bilateral blur
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++)
    {
        float2 curUV = input.uv + i * float2(0, 1.f / float(height));
        finalCol += BlurFunction(curUV, i, centerCol, centerDepth, weight);
    }
    
    return float4(finalCol.xyz / weight, 1.0f);

}