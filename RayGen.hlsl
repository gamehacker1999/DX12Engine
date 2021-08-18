#include "Common.hlsl"
#include "RayGenIncludes.hlsli"
#include "ReStirIncludes.hlsl"

RWStructuredBuffer<uint> newSequences : register(u0, space1);


ConstantBuffer<RayTraceExternData> externData : register(b0);

RWStructuredBuffer<Reservoir> prevFrameRes : register(u0, space3);
RWStructuredBuffer<Reservoir> intermediateReservoir : register(u1, space3);

[shader("raygeneration")] 
void RayGen() 
{
    // Initialize the ray payload
    HitInfo payload;
    payload.color = float3(0, 0, 0);
    payload.rayDepth = 0.0;
    payload.normal = float3(0, 0, 0);


    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);


    float3 pos = gPosition[launchIndex].xyz;
    float3 norm = gNormal[launchIndex].xyz;
    float3 albedo = gAlbedo[launchIndex].xyz;
    float3 metalColor = gRoughnessMetallic[launchIndex].rgb;
    float roughness = gRoughnessMetallic[launchIndex].a;
    
	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, albedo, metalColor);
   
    float3 color = float3(0, 0, 0);
    
    float2 motionVector = motionBuffer[launchIndex].rg;
    
    motionVector.y = 1.f - motionVector.y;
    motionVector = motionVector * 2.f - 1.0f;
    
    float2 screenTexCoord = launchIndex / dims;
    float2 reprojectedTexCoord = screenTexCoord + motionVector;
    
    reprojectedTexCoord *= dims;
    float4 prevValue = gOutput[reprojectedTexCoord];
    
    if (prevValue.x != prevValue.x)
    {
        prevValue = float4(0, 0, 0, 1);
    }
    if (norm.x == 0 && norm.y == 0 && norm.z == 0)
    {
        color = albedo;
        gOutput[launchIndex] = float4(color, 1);
        return;
    }
    else
    {
        uint rndseed = newSequences[launchIndex.y * WIDTH + launchIndex.x];

        if(!externData.doRestir)
        {
            
            int lightToSample = min(int(nextRand(rndseed) * glightCount), glightCount - 1);
            Light light = lights.Load(lightToSample);
            float3 V = normalize(cameraPosition - pos);
            float3 color = float3(0, 0, 0);

            if (light.type == LIGHT_TYPE_DIR)
            {
                color += DirectLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.r, albedo, f0);
            }
            else if (light.type == LIGHT_TYPE_POINT)
            {
                color += PointLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.r, albedo, f0);
            }
            else if (light.type == LIGHT_TYPE_SPOT)
            {
                color += SpotLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.r, albedo, f0);
            }
            
            float pdf = 1.0f / float(glightCount);
            float w = 1.0f / pdf;
            gOutput[launchIndex] = float4(color*w, 1);
            return;
        }
        
        Reservoir prevReservoir = {0,0,0, 0}; // initialize previous reservoir

		// if not first time fill with previous frame reservoir
        if (externData.frameCount != 0)
        {
            //float3 color = float3(1, 0, 0);
            //gOutput[launchIndex] = float4(color, 1);
            //return;
            float4 prevPos = mul(float4(pos, 1.0), mul(externData.prevView, externData.prevProj));
            prevPos /= prevPos.w;
            uint2 prevIndex = launchIndex;
            prevIndex.x = ((prevPos.x + 1.f) / 2.f) * (float) dims.x;
            prevIndex.y = ((1.f - prevPos.y) / 2.f) * (float) dims.y;

            if (prevIndex.x >= 0 && prevIndex.x < dims.x && prevIndex.y >= 0 && prevIndex.y < dims.y)
            {
                prevReservoir = prevFrameRes[prevIndex.y * WIDTH + prevIndex.x];
            }
        }
        
        //Initial gather of information, algorithm 3 of the paper
        Reservoir reservoir = { 0,0,0,0};
        for (int i = 0; i < min(glightCount, 32); i++)
        {
            int lightToSample = min(int(nextRand(rndseed) * glightCount), glightCount - 1);
            Light light = lights.Load(lightToSample);
            float p = 1.0f / float(glightCount);
            
            float L = saturate(normalize(light.position - pos));
            
            float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
            float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.x, albedo, f0);
            
            float w = length(brdfVal) / p;
            UpdateResrvoir(reservoir, lightToSample, w, nextRand(rndseed));
            
            intermediateReservoir[launchIndex.y * WIDTH + launchIndex.x] = reservoir;

        }
        
        Light light = lights.Load(reservoir.y);
            
        float L = saturate(normalize(light.position - pos));
            
        float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
        float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.x, albedo, f0);
            
        float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 1
        
        if(p_hat == 0)
            reservoir.W = 0;
        
        else
            reservoir.W = (1.0 / max(p_hat, 0.00001)) * (reservoir.wsum / max(reservoir.M, 0.000001));
        
        if (ShootShadowRays(pos, L, 1, 1000000)<1.0f)
        {
            reservoir.W = 0;
        }
        
        //Temporal reuse
        Reservoir temporalRes = { 0, 0, 0, 0 };
        
        
        UpdateResrvoir(temporalRes, reservoir.y, p_hat * reservoir.W * reservoir.M, nextRand(rndseed));

        {
            Light light = lights.Load(prevReservoir.y);
            
            float L = saturate(normalize(light.position - pos));
            
            float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
            float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.x, albedo, f0);
            
            if (prevReservoir.M > 20 * reservoir.M)
            {
                prevReservoir.wsum *= 20 * reservoir.M / prevReservoir.M;
                prevReservoir.M = 20 * reservoir.M;
            }
            //prevReservoir.M = min(20.f * reservoir.M, prevReservoir.M); //As described in the paper, clamp the M value to 20*M
            float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 1
            UpdateResrvoir(temporalRes, prevReservoir.y, p_hat * prevReservoir.W * prevReservoir.M, nextRand(rndseed));

        }
        
        temporalRes.M = reservoir.M + prevReservoir.M;
        
        {
            Light light = lights.Load(temporalRes.y);
            
            float L = saturate(normalize(light.position - pos));
            
            float ndotl = saturate(dot(norm.xyz, L)); // lambertian term

	    		// p_hat of the light is f * Le * G / pdf   
            float3 brdfVal = PointLightPBRRaytrace(light, norm, pos, cameraPosition, roughness, metalColor.x, albedo, f0);
            float p_hat = length(brdfVal); // technically p_hat is divided by pdf, but point light pdf is 
            if (p_hat == 0)
            {
                temporalRes.W = 0;
            }
            else
            {
                temporalRes.W = (temporalRes.wsum / temporalRes.M) / p_hat;
            }
            
            reservoir = temporalRes;
        
            intermediateReservoir[launchIndex.y * WIDTH + launchIndex.x] = reservoir;
        
            if(externData.outPutColor)
                gOutput[launchIndex] = float4(brdfVal * reservoir.W, 1);

        }
        


    } 
    //float alpha = 0.3;

    //gOutput[launchIndex] = float4(color * alpha, 1.0f) + (float4(prevValue.xyz * (1.f - alpha), 1.0f));
    
}
