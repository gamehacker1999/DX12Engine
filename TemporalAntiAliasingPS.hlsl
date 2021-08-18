#include "Common.hlsl"
#include "Utils.hlsli"
Texture2D currentFrame: register(t0);
Texture2D previousFrame: register(t1);
Texture2D velocityBuffer : register(t2);
Texture2D depthBuffer : register(t3);
SamplerState basicSampler: register(s0);
SamplerState pointSampler : register(s1);

cbuffer ExternalData : register(b0)
{
    int frameNum;
};

cbuffer TAAExternData : register(b1)
{
   matrix prevView;
   matrix prevProjection;
   matrix inverseProjection;
   matrix inverseView;
};

struct VertexToPixel
{
	float4 position:  SV_POSITION;
	float2 uv:		  TEXCOORD;
};

float3 CalcPositionFromDepth(float2 tex, float depth)
{
    float4 position = float4(tex.x * (2) - 1, tex.y * (-2) + 1, depth, 1.0f);

    position = mul(position, inverseProjection);
    position = mul(position, inverseView);

    return position.xyz / position.w;
}



float4 main(VertexToPixel input) : SV_TARGET
{

	float3 currentColor = currentFrame.SampleLevel(pointSampler, input.uv, 0);
	float3 previousColor = previousFrame.SampleLevel(pointSampler, input.uv, 0);
    int w, h;
    currentFrame.GetDimensions(w, h);
    float2 pixelSize = float2(1.0 / float(w), 1.0 / float(h)); //Need to pass this later

	if (frameNum == 0)
	{
		return float4(currentColor, 1.0f);
	}

    float3 final = lerp(previousColor, currentColor, 0.05);
    //return float4(final, 1.0);

    const float4 nbh[9] =
    {
        ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y - pixelSize.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y + pixelSize.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y - pixelSize.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y + pixelSize.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y - pixelSize.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y)))),
		ConvertToYCoCg(TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y + pixelSize.y)))),
    };
    const float4 color = nbh[4];
    
    const float4 gaussian[9] =
    {
        TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y - pixelSize.y))) * (1.0f / 16.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y))) * (1.0f / 8.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x - pixelSize.x, input.uv.y + pixelSize.y))) * (1.0f / 16.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y - pixelSize.y))) * (1.0f / 8.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y))) * (1.0f / 4.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x, input.uv.y + pixelSize.y))) * (1.0f / 8.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y - pixelSize.y))) * (1.0f / 16.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y))) * (1.0f / 8.0f),
		TonemapColor(currentFrame.Sample(pointSampler, float2(input.uv.x + pixelSize.x, input.uv.y + pixelSize.y))) * (1.0f / 16.0f),
    };
    
    float3 blurredColor = float3(0, 0, 0);
    
    for (int i = 0; i < 9;i++)
    {
        blurredColor += gaussian[i];
    }

    const float4 minimum = min(min(min(min(min(min(min(min(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 maximum = max(max(max(max(max(max(max(max(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 average = (nbh[0] + nbh[1] + nbh[2] + nbh[3] + nbh[4] + nbh[5] + nbh[6] + nbh[7] + nbh[8]) * 1.0f/9.0f;
	
	//sample velocity uv  
    float2 vel = velocityBuffer.SampleLevel(pointSampler, input.uv, 0).xy;
        
    float2 previousCoordinate = input.uv;
    
    if (vel.x >= 1)
    {
        float3 currentPosition = CalcPositionFromDepth(input.uv, depthBuffer.Sample(basicSampler, input.uv).r);
        float4 previousPosition = mul(float4(currentPosition, 1.0f), prevView);
        previousPosition = mul(previousPosition, prevProjection);
    
        previousCoordinate = (previousPosition.xy / previousPosition.w) * float2(0.5f, -0.5f)+0.5;
    
    }
    
    else
     previousCoordinate += vel;
    
    
	
    float2 historySize = float2(WIDTH, HEIGHT);
    float4 history = ConvertToYCoCg(TonemapColor(previousFrame.Sample(basicSampler, previousCoordinate)));
    
    if(history.x != history.x)
    {   
        history = float4(0, 0, 0, 1);
    }
	
    
    const float3 origin = history.rgb - 0.5f * (minimum.rgb + maximum.rgb);
    const float3 direction = average.rgb - history.rgb;
    const float3 extents = maximum.rgb - 0.5f * (minimum.rgb + maximum.rgb);

    history = lerp(history, average, saturate(IntersectAABB(origin, direction, extents)));

    float blendFactor = 0.05f;
    float impulse = abs(color.x - history.x) / max(color.x, max(history.x, minimum.x));
    float factor = lerp(blendFactor * 0.8f, blendFactor * 2.0f, impulse * impulse);
    
    if(factor == 1.f)
        return float4(InvertTonemapColor(float4(blurredColor, 1.0f)));
    
    float4 finalColor = ConvertToRGBA(InvertTonemapColor(lerp(history, color, factor)));
    
    return finalColor;

}