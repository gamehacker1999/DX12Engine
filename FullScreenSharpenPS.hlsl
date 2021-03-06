#include "Common.hlsl"
Texture2D taaOutput : register(t0);
SamplerState pointSampler : register(s0);

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};


float4 main(VertexToPixel input) : SV_TARGET
{
    float2 pixelSize = float2(1.0 / float(WIDTH), 1.0 / float(HEIGHT));
    float4 center = taaOutput.Sample(pointSampler, input.uv);
    float4 left = taaOutput.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y));
    float4 down = taaOutput.Sample(pointSampler, float2(input.uv.x, input.uv.y + pixelSize.y));
    float4 up = taaOutput.Sample(pointSampler, float2(input.uv.x, input.uv.y - pixelSize.y));
    float4 right = taaOutput.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y));
    float4 topRight = taaOutput.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y - pixelSize.y));
    float4 topLeft = taaOutput.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y - pixelSize.y));
    float4 bottomRight = taaOutput.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y + pixelSize.y));
    float4 bottomLeft = taaOutput.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y + pixelSize.y));


    return (center); // + (4 * center) - up - down - left - right);
    //-topLeft - topRight - bottomLeft - bottomRight);
}