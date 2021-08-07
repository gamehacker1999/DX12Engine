#include "Common.hlsl"
#include "RTUtils.hlsli"
#include "ReStirIncludes.hlsl"


// G-Buffer
RWTexture2D<float4> gBufferPos : register(u0);
RWTexture2D<float4> gBufferNorm : register(u1);
RWTexture2D<float4> gBufferDif : register(u2);
RWTexture2D<float4> gBufferRoughnessMetal : register(u3);
RWTexture2D<float4> outColor: register(u4);

RWStructuredBuffer<Reservoir> reservoirs : register(u5);
RWStructuredBuffer<uint> newSequences : register(u6);
RWStructuredBuffer<Reservoir> outReservoir : register(u7);

cbuffer RestirData : register(b0)
{
    float3 camPos;
};


#define NUM_NEIGHBORS 5
#define SAMPLE_RADIUS 5

inline float halton(int i, int b)
{
	/* Creates a halton sequence of values between 0 and 1.
	https://en.wikipedia.org/wiki/Halton_sequence
	Used for jittering based on a constant set of 2D points. */
	float f = 1.0;
	float r = 0.0;
	while (i > 0)
	{
		f = f / float(b);
		r = r + f * float(i % b);
		i = i / b;
	}
	return r;
}

[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    
    uint2 pixelPos = DTid.xy;
    
    Reservoir r = reservoirs[pixelPos.y * WIDTH + pixelPos.x];
    Reservoir reservoirNew = { 0, 0, 0, 0 };
  
    if (outColor[pixelPos].x != outColor[pixelPos].x)
    {
        outColor[pixelPos] = float4(0, 0, 0, 1);
        return;

    }
	
    uint rndSeed = newSequences[pixelPos.y * WIDTH + pixelPos.x];
    
    float3 pos = gBufferPos[pixelPos].xyz;
    float3 norm = gBufferNorm[pixelPos].xyz;
    float3 albedo = gBufferDif[pixelPos].xyz;
    float3 metalColor = gBufferRoughnessMetal[pixelPos].rgb;
    float roughness = gBufferRoughnessMetal[pixelPos].a;
    
    float3 f0 = float3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, albedo, metalColor);
    
    //Spatial reuse pass
    float lightSamplesCount = r.M;
    
    if (norm.x == 0 && norm.y == 0 && norm.z == 0)
    {
        outColor[pixelPos] = float4(albedo, 1);
        return;
    }
    
     {
    
        //Adjusting the final weight of reservoir Equation 6 in paper
        Light light = lights.Load(r.y);
        float L = saturate(normalize(light.position - pos));
            
        float ndotl = saturate(dot(norm.xyz, L)); // lambertian term
        
	    		// p_hat of the light is f * Le * G / pdf   
        float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, camPos, roughness, metalColor.x, albedo, f0);
        float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 1
        UpdateResrvoir(reservoirNew, r.y, p_hat * r.W * r.M, nextRand(rndSeed));
        
        //outReservoir[pixelPos.y * WIDTH + pixelPos.x] = r;
        //
        //outColor[pixelPos] = float4(brdfVal * r.W, 1);
        //
        //return;
    }

    uint2 neighborOffset;
    uint2 neighborIndex;
    for (int i = 0; i < NUM_NEIGHBORS;i++)
    {
        float2 xi = Hammersley(i, NUM_NEIGHBORS);

        float radius = SAMPLE_RADIUS * nextRand(rndSeed);
        float angle = 2.0f * M_PI * nextRand(rndSeed);
        
        float2 neighborIndex = pixelPos;
        
        neighborIndex.x += radius * cos(angle);
        neighborIndex.y += radius * sin(angle);
        
        if (neighborIndex.x < 0 || neighborIndex.x >= WIDTH || neighborIndex.y < 0 || neighborIndex.y >= HEIGHT)
        {
            continue;
        }
        
        // The angle between normals of the current pixel to the neighboring pixel exceeds 25 degree		
        if (acos(dot(gBufferNorm[pixelPos].xyz, gBufferNorm[neighborIndex].xyz)) > 0.4)
        {
            continue;
        }
        
		// Exceed 10% of current pixel's depth
        if (gBufferNorm[neighborIndex].w > 1.1 * gBufferNorm[pixelPos].w || gBufferNorm[neighborIndex].w < 0.9 * gBufferNorm[pixelPos].w)
        {
            continue;
        }
        
        
      // neighborOffset.x = int(nextRand(rndSeed) * SAMPLE_RADIUS * 2.f) - SAMPLE_RADIUS;
      // neighborOffset.y = int(nextRand(rndSeed) * SAMPLE_RADIUS * 2.f) - SAMPLE_RADIUS;
      //
      // neighborIndex.x = max(0, min(WIDTH - 1, WIDTH + neighborOffset.x));
      // neighborIndex.y = max(0, min(HEIGHT - 1, HEIGHT + neighborOffset.y));

        Reservoir neighborRes = reservoirs[neighborIndex.y * WIDTH + neighborIndex.x];
        
        Light light = lights.Load(neighborRes.y);
        
        float L = saturate(normalize(light.position - pos));
            
        float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
        float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, camPos, roughness, metalColor.x, albedo, f0);
        float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 1
        UpdateResrvoir(reservoirNew, neighborRes.y, p_hat * neighborRes.W * neighborRes.M, nextRand(rndSeed));
        
        lightSamplesCount += neighborRes.M;
    }
    
    reservoirNew.M = lightSamplesCount;

    //Adjusting the final weight of reservoir Equation 6 in paper
    Light light = lights.Load(reservoirNew.y);
    float L = saturate(normalize(light.position - pos));
            
    float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
    float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, camPos, roughness, metalColor.x, albedo, f0);
    float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 1
    reservoirNew.W = (1.0 / max(p_hat, 0.00001)) * (reservoirNew.wsum / max(reservoirNew.M, 0.0001));
    
    outReservoir[pixelPos.y * WIDTH + pixelPos.x] = reservoirNew;
    
    outColor[pixelPos] = float4(brdfVal * reservoirNew.W, 1);

}