#include "Common.hlsl"
#include "Utils.hlsli"
Texture2D currentFrame: register(t0);
Texture2D previousFrame: register(t1);
Texture2D velocityBuffer : register(t2);
SamplerState basicSampler: register(s0);
SamplerState pointSampler : register(s1);

cbuffer ExternalData: register(b0)
{
	int frameNum;
};

struct VertexToPixel
{
	float4 position:  SV_POSITION;
	float2 uv:		  TEXCOORD;
};

float4 main(VertexToPixel input) : SV_TARGET
{

	float3 currentColor = currentFrame.SampleLevel(pointSampler, input.uv, 0);
	float4 previousColor = previousFrame.SampleLevel(pointSampler, input.uv, 0);
    float2 pixelSize = float2(1.0 / float(WIDTH), 1.0 / float(HEIGHT)); //Need to pass this later

	if (frameNum == frameNum)
	{
		return float4(currentColor, 1.0f);
	}

    //float3 final = lerp(previousColor, currentColor, 0.05);
    //return float4(final, 1.0);

    const float4 nbh[9] =
    {
        ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y - pixelSize.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y + pixelSize.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y - pixelSize.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y + pixelSize.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y - pixelSize.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y))),
		ConvertToYCoCg(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y + pixelSize.y))),
    };
    const float4 color = nbh[4];


    const float4 minimum = min(min(min(min(min(min(min(min(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 maximum = max(max(max(max(max(max(max(max(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 average = (nbh[0] + nbh[1] + nbh[2] + nbh[3] + nbh[4] + nbh[5] + nbh[6] + nbh[7] + nbh[8]) * 1.0f/9.0f;
	
	//sample velocity uv  
    float2 vel = velocityBuffer.SampleLevel(pointSampler, input.uv, 0).xy;
    float2 historyUV = input.uv + vel;
	
    float2 historySize = float2(WIDTH, HEIGHT);
    float4 history = ConvertToYCoCg(SampleTextureCatmullRom(previousFrame, basicSampler, historyUV, historySize));
	
    const float3 origin = history.rgb - 0.5f * (minimum.rgb + maximum.rgb);
    const float3 direction = average.rgb - history.rgb;
    const float3 extents = maximum.rgb - 0.5f * (minimum.rgb + maximum.rgb);

    history = lerp(history, average, saturate(IntersectAABB(origin, direction, extents)));

    float blendFactor = 0.1f;

    float impulse = abs(color.x - history.x) / max(color.x, max(history.x, minimum.x));
    float factor = lerp(blendFactor * 0.8f, blendFactor, impulse * impulse);
	
    return ConvertToRGBA(lerp(history, color, factor));

}