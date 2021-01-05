Texture2D taaOutput : register(t0);
SamplerState pointSampler : register(s0);
#define FXAA_SPAN_MAX	8.0
#define FXAA_REDUCE_MUL 1.0/8.0
#define FXAA_REDUCE_MIN 1.0/128.0
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

//========================================================================================

float4 main(VertexToPixel input) : SV_TARGET
{

    float2 add = float2(1.0, 1.0) / float2(1280, 720);
			
    float3 rgbNW = taaOutput.Sample(pointSampler, (input.uv + float2(-add.x, -add.y)));
    float3 rgbNE = taaOutput.Sample(pointSampler, (input.uv + float2(add.x, -add.y)));
    float3 rgbSW = taaOutput.Sample(pointSampler, (input.uv + float2(-add.x, add.y)));
    float3 rgbSE = taaOutput.Sample(pointSampler, (input.uv + float2(add.x, add.y)));
    float3 rgbM = taaOutput.Sample(pointSampler, (input.uv));
	
    float3 luma = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);
	
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
	
	
    float dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	  
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	

    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
		  max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
		  dir * rcpDirMin)) * add;

		
    float3 rgbA = (1.0 / 2.0) * (taaOutput.Sample(pointSampler, (input.uv + dir * (1.0 / 3.0 - 0.5))).rgb +
							 taaOutput.Sample(pointSampler, (input.uv + dir * (2.0 / 2.0 - 0.5))).rgb);
	
    float3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) *
		(taaOutput.Sample(pointSampler, (input.uv.xy + dir * (0.0 / 3.0 - 0.5))).rgb +
		 taaOutput.Sample(pointSampler, (input.uv.xy + dir * (3.0 / 3.0 - 0.5))).rgb);
	
    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return float4(rgbA, 1);
    }
    else
    {
        return float4(rgbB, 1);
    }
}