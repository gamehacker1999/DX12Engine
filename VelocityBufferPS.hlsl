#include "Common.hlsl"
struct VertexToPixel
{
	float4 position : SV_POSITION;
    float4 prevPosition : LOL;

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
    return float2(0.5f * cs.x, 0.5f * cs.y) + 0.5f;
}


float2 main(VertexToPixel input) : SV_TARGET
{
    float2 initPos = ((input.position.xy / input.position.w) - cur) * 0.5 + 0.5;
	initPos.y *= -1;
    float2 prevPos = ((input.prevPosition.xy / input.prevPosition.w) - prev) * 0.5 + 0.5;
	prevPos.y *= -1;
	
    float2 result = ClipSpaceToTextureSpace(input.position, cur) - ClipSpaceToTextureSpace(input.prevPosition, prev);
	
    return result;
}