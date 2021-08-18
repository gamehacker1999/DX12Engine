#include "Common.hlsl"
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float4 curPosition : POSITION;
    float4 prevPosition : POSITION1;

};
cbuffer Jitters : register(b0)
{
    float2 cur;
    float2 prev;
}

float2 ClipSpaceToTextureSpace(float4 clipSpace, float2 jitter)
{
    float2 cs = clipSpace.xy / clipSpace.w;
    cs -= jitter;

    return cs;
}


float2 main(VertexToPixel input) : SV_TARGET
{
    float2 result = (ClipSpaceToTextureSpace(input.prevPosition, prev) - ClipSpaceToTextureSpace(input.curPosition, cur)) * float2(0.5f, -0.5f);
	
    return result;
}