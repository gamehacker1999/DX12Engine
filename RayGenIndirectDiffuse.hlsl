#include "RayGenIncludes.hlsli"
#include "ReStirIncludes.hlsl"

RWStructuredBuffer<uint> newSequences : register(u0, space1);

ConstantBuffer<RayTraceExternData> externData : register(b0);

RWStructuredBuffer<GIReservoir> prevFrameRes : register(u0, space3);
RWStructuredBuffer<GIReservoir> intermediateReservoir : register(u1, space3);
float IntersectAABB(float3 origin, float3 direction, float3 extents)
{
    float3 reciprocal = rcp(direction);
    float3 minimum = (extents - origin) * reciprocal;
    float3 maximum = (-extents - origin) * reciprocal;
	
    return max(max(min(minimum.x, maximum.x), min(minimum.y, maximum.y)), min(minimum.z, maximum.z));
}

float3 TemporalReprojection(uint2 launchDims, float4 history, float4 color)
{
    int w, h;
    gIndirectDiffuseOutput.GetDimensions(w, h);
    float2 pixelSize = float2(1.0 / float(w), 1.0 / float(h)); //Need to pass this later
    const float4 nbh[9] =
    {
        ((gIndirectDiffuseOutput[uint2(launchDims.x - 1, launchDims.y - 1)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x - 1, launchDims.y)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x - 1, launchDims.y + 1)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x, launchDims.y - 1)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x, launchDims.y)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x, launchDims.y + 1)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x + 1, launchDims.y - 1)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x + 1, launchDims.y)])),
		((gIndirectDiffuseOutput[uint2(launchDims.x + 1, launchDims.y + 1)]))
    };

    const float4 minimum = min(min(min(min(min(min(min(min(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 maximum = max(max(max(max(max(max(max(max(nbh[0], nbh[1]), nbh[2]), nbh[3]), nbh[4]), nbh[5]), nbh[6]), nbh[7]), nbh[8]);
    const float4 average = (nbh[0] + nbh[1] + nbh[2] + nbh[3] + nbh[4] + nbh[5] + nbh[6] + nbh[7] + nbh[8]) * 1.0f / 9.0f;
    
    const float3 origin = history.rgb - 0.5f * (minimum.rgb + maximum.rgb);
    const float3 direction = average.rgb - history.rgb;
    const float3 extents = maximum.rgb - 0.5f * (minimum.rgb + maximum.rgb);

    history = lerp(history, average, saturate(IntersectAABB(origin, direction, extents)));

    float blendFactor = 1.0f;

    float impulse = abs(color.x - history.x) / max(color.x, max(history.x, minimum.x));
    float factor = lerp(blendFactor * 0.5f, blendFactor * 2.0f, impulse * impulse);
	
    if(factor == 1)
    {
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.color = float3(0, 0, 0);
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.sampleNormal = float3(0, 0, 0);
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.visibleNormal = float3(0, 0, 0);
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.RandomNums = float3(0, 0, 0);
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.samplePos = float3(0, 0, 0);
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].sample.visiblePos = float3(0, 0, 0);
        
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].M = 0;
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].wsum = 0;
        prevFrameRes[launchDims.y * WIDTH + launchDims.x].W = 0;



    }
    return ((lerp(history, color, factor)));
}

[shader("raygeneration")]
void IndirectDiffuseRayGen()
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

// Sample the shadow texture from last frame
// Make sure to use a sampler with border color wrapping to avoid artifacts
// The first channel of the output contains shadows for the first/visibility bounce
// The second channel of the output contains shadows in reflections
    float4 prevValue = gIndirectDiffuseOutput[reprojectedTexCoord];
    uint rndseed = newSequences.Load((launchIndex.y) * WIDTH + (launchIndex.x));

    if (prevValue.x != prevValue.x)
    {
        prevValue = float4(0, 0, 0, 1);
        return;
    }

    if (norm.x == 0 && norm.y == 0 && norm.z == 0)
    {
        color = albedo;
        gIndirectDiffuseOutput[launchIndex] = float4(color, 1.0);
        return;
    }
    else
    {

        
        if (!externData.doRestirGI)
        {
            float3 V = normalize(cameraPosition - pos);

            float3 indirectLight = float3(0, 0, 0);
        
            for (int i = 0; i < 1; i++)
            {
            // Do indirect lighting for global illumination
                indirectLight += IndirectDiffuseLighting(rndseed, pos, norm, V, metalColor.r,
	        albedo, f0, roughness, 0);
            }
        

        
            indirectLight /= 1;
            color += indirectLight;
            
            
            float alpha = 0.3;
            gIndirectDiffuseOutput[launchIndex] = float4(lerp(prevValue.xyz, color, alpha), 1);
            //gIndirectDiffuseOutput[launchIndex] = float4(color, 1.0);
            
            return;
        }
        
        //Generate the initial Samples Alg 2
        
        float3 randomVars = float3(nextRand(rndseed), nextRand(rndseed), 0);
        float3 L = GetCosHemisphereSample(randomVars.x, randomVars.y, norm);
        L = normalize(L);
        HitInfo giPayload = { float3(0, 0, 0), 0, rndseed, pos, norm, albedo };
       
        RayDesc ray;
        ray.Origin = pos;
        ray.Direction = L;
        ray.TMin = 0.01;
        ray.TMax = 100000;
        
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
        
        Sample newSample;
        newSample.visibleNormal = norm;
        newSample.visiblePos = pos;
        newSample.sampleNormal = giPayload.normal;
        newSample.samplePos = giPayload.currentPosition;
        newSample.color = giPayload.color;
        newSample.RandomNums = randomVars;
        
        //Creating the temporal buffer Alg 3
        
        //Applying temporal reuse 
        GIReservoir prevReservoir =
        {
            newSample
            , 0, 0, 0
        };
        //prevFrameRes[launchIndex.y * WIDTH + launchIndex.x] = prevReservoir;
        
        //if(externData.frameCount == 0)
            //return;
        float4 history;
        // if not first time fill with previous frame reservoir
        if (externData.frameCount != 0)
        {

            float4 prevPos = mul(float4(pos, 1.0), mul(externData.prevView, externData.prevProj));
            prevPos /= prevPos.w;
            uint2 prevIndex = launchIndex;
            prevIndex.x = ((prevPos.x + 1.f) / 2.f) * (float) dims.x;
            prevIndex.y = ((1.f - prevPos.y) / 2.f) * (float) dims.y;

            if (prevIndex.x >= 0 && prevIndex.x < dims.x && prevIndex.y >= 0 && prevIndex.y < dims.y)
            {
                prevReservoir = prevFrameRes[prevIndex.y * WIDTH + prevIndex.x];
            }
            
            history= gIndirectDiffuseOutput[prevIndex];

        }

        float sourcePDF = saturate(dot(norm, normalize(L))) / M_PI; //PDF of diffuse ray is n.l/pi
        float w = length(giPayload.color * albedo); //equation 5 + 9 from the paper the cosine term and the pi term get cancelled out due to the pdf
        //gIndirectDiffuseOutput[launchIndex] = float4(giPayload.color*albedo, 1.0);
        //return;
        UpdateGIResrvoir(prevReservoir, newSample, w, nextRand(rndseed));
        
        if (prevReservoir.M > 30)
        {
            prevReservoir.wsum *= 30 / prevReservoir.M;
            prevReservoir.M = 30;
        }
       
            
        //else
        {
        
            float3 newL = prevReservoir.sample.samplePos - prevReservoir.sample.visiblePos;
            newL = normalize(newL);
       
            float3 brdfVal = prevReservoir.sample.color * albedo / M_PI * saturate(dot(prevReservoir.sample.visibleNormal, newL));
            float p_hat = length(brdfVal);
       
            if (p_hat == 0)
                prevReservoir.W = 0;
            else
                prevReservoir.W = (prevReservoir.wsum) / (prevReservoir.M * p_hat);
        
            prevFrameRes[launchIndex.y * WIDTH + launchIndex.x] = prevReservoir;
            float3 finalBrdfVal = albedo / M_PI * (prevReservoir.sample.color) * saturate(dot(newL, prevReservoir.sample.visibleNormal));

            gIndirectDiffuseOutput[launchIndex] = float4(finalBrdfVal * prevReservoir.W, 1.0);
        //}
         //if (externData.frameCount != 0)
         //{ 
         //    gIndirectDiffuseOutput[launchIndex] = float4(TemporalReprojection(launchIndex, history, gIndirectDiffuseOutput[launchIndex]), 1);
         //}

        }

    }
}
