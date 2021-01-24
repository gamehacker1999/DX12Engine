#include "Common.hlsl"
Texture2D inputTex : register(t0);
SamplerState pointSampler : register(s0);

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};


float4 main(VertexToPixel input) : SV_TARGET
{
    return inputTex.Sample(pointSampler, input.uv);
    //return (float4(0,0,0,1));
}