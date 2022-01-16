#include "Common.hlsl"
#include "RTUtils.hlsli"
#include "ReStirIncludes.hlsl"


// G-Buffer
RWTexture2D<float4> gBufferPos : register(u0);
RWTexture2D<float4> gBufferNorm : register(u1);
RWTexture2D<float4> gBufferDif : register(u2);
RWTexture2D<float4> gBufferRoughnessMetal : register(u3);
RWTexture2D<float4> outColor : register(u4);

RWStructuredBuffer<GIReservoir> temporalRes : register(u5);
RWStructuredBuffer<uint> newSequences : register(u6);
RWStructuredBuffer<GIReservoir> spatialRes : register(u7);

cbuffer RestirData : register(b0)
{
    float3 camPos;
};


#define NUM_NEIGHBORS 3
#define SAMPLE_RADIUS 30

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

float CalculateJacobian(Sample q, Sample r)
{
     
    float3 normal = q.sampleNormal;
    
    
    float cosPhi1 = abs(dot(normalize(r.visiblePos - q.samplePos), normal));
    float cosPhi2 = abs(dot(normalize(q.visiblePos - q.samplePos), normal));

    //cosPhi1 = clamp(cosPhi1, -0.5, 0.5);
    //cosPhi2 = clamp(cosPhi2, -0.5, 0.5);

    float term1 = cosPhi1 / max(0.0001, cosPhi2);
    
    float num = length((q.visiblePos - q.samplePos));
    num *= num;

    float denom = length((r.visiblePos - q.samplePos));
    
    denom *= denom;
    
    //num = clamp(num, 0.1, 1);
    //denom = clamp(denom, 0.5, 1);
    
    float term2 = num / max(denom,0.001);
    
    //term2 = clamp(term2, 0.01, 1);
    float jacobian =term1*term2;
    
    jacobian = clamp(jacobian, 0.06, 1);
    
    return jacobian;

}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint2 pixelPos = DTid.xy;
    
    GIReservoir r = spatialRes[pixelPos.y * uint(WIDTH) + pixelPos.x];
  
    if (outColor[pixelPos].x != outColor[pixelPos].x)
    {
        outColor[pixelPos] = float4(0, 0, 0, 1);
        return;

    }
	
    uint rndSeed = newSequences[pixelPos.y * uint(WIDTH) + pixelPos.x];
    
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

    uint2 neighborOffset;
    uint2 neighborIndex;
    int z = 0;
    for (int i = 0; i < NUM_NEIGHBORS; i++)
    {
        float2 xi = Hammersley(i, NUM_NEIGHBORS);
        
        float radius = SAMPLE_RADIUS * nextRand(rndSeed);
        float angle = 2.0f * M_PI * nextRand(rndSeed);
        
        float2 neighborIndex = pixelPos;
        
        neighborIndex.x += radius * cos(angle);
        neighborIndex.y += radius * sin(angle);
         
        uint2 u_neighbor = uint2(neighborIndex);
        if (u_neighbor.x < 0 || u_neighbor.x >= WIDTH || u_neighbor.y < 0 || u_neighbor.y >= HEIGHT)
        {
            continue;
        }
        
        // The angle between normals of the current pixel to the neighboring pixel exceeds 25 degree		
        if ((dot(gBufferNorm[pixelPos].xyz, gBufferNorm[u_neighbor].xyz)) < 0.906)
        {
            continue;
        }
        
	    //  Exceed 10% of current pixel's depth
        if (gBufferNorm[u_neighbor].w > 1.1 * gBufferNorm[pixelPos].w || gBufferNorm[u_neighbor].w < 0.9 * gBufferNorm[pixelPos].w)
        {
            continue;
        }
        
        GIReservoir neighborRes = temporalRes[u_neighbor.y * uint(WIDTH) + u_neighbor.x];
        
        float3 newL = neighborRes.sample.samplePos - neighborRes.sample.visiblePos;
        newL = normalize(newL);
        
        float3 brdfVal = neighborRes.sample.color * (albedo / M_PI) * saturate(dot(neighborRes.sample.visibleNormal, newL));
        
        float jacobian = CalculateJacobian( neighborRes.sample, r.sample);
        
        float p_hat = length(brdfVal);
        
        p_hat /= jacobian;
        
        //TODO DO visibility testing here
        {
    
	        //creating the rayDescription
            RayDesc ray;
            ray.Origin = pos;
            ray.Direction = newL;
            ray.TMin = 0.01;
            ray.TMax = 100000;
	
		    // Instantiate ray query object.
		    // Template parameter allows driver to generate a specialized
		    // implementation.
            RayQuery < RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > q;
	
	        // Set up a trace.  No work is done yet.
            q.TraceRayInline(
            SceneBVH,
            RAY_FLAG_NONE, // OR'd with flags above
            0,
            ray);

		    // Proceed() below is where behind-the-scenes traversal happens,
		    // including the heaviest of any driver inlined code.
		    // In this simplest of scenarios, Proceed() only needs
		    // to be called once rather than a loop:
		    // Based on the template specialization above,
		    // traversal completion is guaranteed.
            q.Proceed();
		    // Examine and act on the result of the traversal.
		    // Was a hit committed?
            if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
            {
                float3 pos = q.WorldRayOrigin() + q.CommittedRayT() * q.WorldRayDirection();
                
                if(pos.x!=neighborRes.sample.samplePos.x)
                    p_hat = 0;
            }
        }
        
        UpdateGIResrvoir(r, neighborRes.sample, p_hat * neighborRes.W * neighborRes.M, nextRand(rndSeed));
         
        lightSamplesCount += neighborRes.M;
    }
    
    r.M = lightSamplesCount;

    float3 newL = r.sample.samplePos - r.sample.visiblePos;
    newL = normalize(newL);
        
    float3 brdfVal = r.sample.color * (albedo / M_PI) * saturate(dot(r.sample.visibleNormal, newL));
    float p_hat = length(brdfVal);
    
    if (r.M > 500)
    {
        r.wsum *= 500 / r.M;
        r.M = 500;
    }
    
    if (p_hat == 0)
        r.W = 0;
    else
        r.W = (1.0 / max(p_hat, 0.00001)) * (r.wsum / max(r.M, 0.0001));
    
    spatialRes[pixelPos.y * uint(WIDTH) + pixelPos.x] = r;
    
    outColor[pixelPos] = float4(brdfVal * r.W, 1);

}