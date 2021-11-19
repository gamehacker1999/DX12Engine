#include "Common.hlsl"

#include "HitGroupIncludes.hlsli"

#define sphere_mult 0.05
// inv_sphere_mult = 1/sphere_mult
#define inv_sphere_mult 20.


// this noise lookup inspired by iq's 3D noise lookup in his clouds shader

float hash(float n)
{
    return frac(sin(n) * 43758.5453);
}

float noise(float3 x)
{
    // The noise function returns a value in the range -1.0f -> 1.0f

    float3 p = floor(x);
    float3 f = frac(x);

    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;

    return lerp(lerp(lerp(hash(n + 0.0), hash(n + 1.0), f.x),
                   lerp(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               lerp(lerp(hash(n + 113.0), hash(n + 114.0), f.x),
                   lerp(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}


float fbm(float3 p)
{
    return abs(
           noise(p * 1.) * .6) +
           noise(p * 2.) * .3 +
           noise(p * 4.) * .25 +
           noise(p * 8.) * .125;
}
// makes a sphere
// x - length(p) is inverse distance function 
// points with length less than x get positive values, outside of x radius values become negative
// high enough values from the FBM can outweigh the negative distance values
float scene(float3 p)
{
    return .5 - length(p) * 0.05 + fbm(p * .0321);
}


float3 Translate(float3 pos, float3 translate)
{
    return pos -= translate;
}

// Taken from https://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdSphere(float3 p, float3 origin, float s)
{
    p = Translate(p, origin);
    return length(p) - s;
}

// Taken from https://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdPlane(float3 p)
{
    return p.y;
}

float sdSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float QueryVolumetricDistanceField(in float3 pos)
{
    float3 fbmCoord = (pos + 2.0 * float3(totalTime, 0.0, 1)) / 1.5f;
    float sdfValue = sdSphere(pos, float3(-8.0, 2.0 + 20.0 * sin(totalTime), -1), 5.6);
    sdfValue = sdSmoothUnion(sdfValue, sdSphere(pos, float3(8.0, 8.0 + 12.0 * cos(totalTime), 3), 5.6), 3.0f);
    sdfValue = sdSmoothUnion(sdfValue, sdSphere(pos, float3(5.0 * sin(totalTime), 3.0, 0), 8.0), 3.0) + 7.0 * fbm(fbmCoord / 3.2);
    sdfValue = sdSmoothUnion(sdfValue, sdPlane(pos + float3(0, 0.4, 0)), 22.0);
    return sdfValue;
}

//http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/#surface-normals-and-lighting
float3 EstimateNormal(float3 p)
{
    float EPSILON = 0.1;
    return normalize(float3(
        QueryVolumetricDistanceField(float3(p.x + EPSILON, p.y, p.z)) - QueryVolumetricDistanceField(float3(p.x - EPSILON, p.y, p.z)),
        QueryVolumetricDistanceField(float3(p.x, p.y + EPSILON, p.z)) - QueryVolumetricDistanceField(float3(p.x, p.y - EPSILON, p.z)),
        QueryVolumetricDistanceField(float3(p.x, p.y, p.z + EPSILON)) - QueryVolumetricDistanceField(float3(p.x, p.y, p.z - EPSILON))
    ));
}

bool SphereHit(float3 p)
{
    return distance(p, float3(0,0,0)) < 1;
}

float RaymarchHit(inout float3 position, float3 direction, out float3 normal)
{
    float opaqueVisiblity = 1.0f;
    const float marchSize = 0.6f;
    float t = 0;
    for (int i = 0; i < 64; i++)
    {
        float distance = QueryVolumetricDistanceField(position);
        t += distance;
        if (distance < 0.01)
        {
            normal = EstimateNormal(position);
            return t;

        }
        position += direction * distance;
    }
    return 0;
}

float BeerLambert(float absorption, float dist)
{
    return exp(-absorption * dist);
}

float GetFogDensity(float3 position, float sdfDistance)
{
    const float maxSDFMultiplier = fogDensity;
    bool insideSDF = sdfDistance < 0.0;
    float sdfMultiplier = insideSDF ? min(abs(sdfDistance), maxSDFMultiplier) : 0.0;
 
#if UNIFORM_FOG_DENSITY
    return sdfMultiplier;
#else
    return sdfMultiplier * abs(fbm(position / 6.0) + 0.5);
#endif
}
// this noise, including the 5.58... scrolling constant are from Jorge Jimenez
float InterleavedGradientNoise(float2 pixel, int frame)
{
    pixel += (float(frame) * 5.588238f);
    return frac(52.9829189f * frac(0.06711056f * float(pixel.x) + 0.00583715f * float(pixel.y)));
}

float GetLightVisibility(float3 rayOrigin, float3 rayDirection, float maxT, float maxSteps, float marchSize)
{
    float lightVisibility = 1.0f;
    
    float t = 0.0f;
    
    for (int i = 0; i < maxSteps;i++)
    {
        t += marchSize;
        if (t >= maxT)
            break;
        float3 position = rayOrigin + t * rayDirection * ((i + InterleavedGradientNoise(DispatchRaysIndex().xy, 1)) / maxSteps);
        if (QueryVolumetricDistanceField(position)<0.0f)
        {
            lightVisibility *= BeerLambert(0.5, marchSize);

        }

    }
    
    return lightVisibility;
}


[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    payload.rayDepth++;

    float3 position;
    float4 surfaceColor;
    float3 normal;
    float3 metalColor;
    float roughness;
    
    GetPrimitiveProperties(surfaceColor, position, normal, metalColor, roughness, attrib);
    float3 V = normalize(position - cameraPosition);
    float3 f0 = float3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, surfaceColor, metalColor);
     // Do indirect lighting for global illumination
    float3 color = float3(0, 0, 0);
    
    payload.tCurrent = RayTCurrent();
    
    color += DirectLighting(payload.rndseed, position, normal, V, metalColor.r, surfaceColor, f0, roughness);
    
    //if(payload.rayDepth < 2)
    //{
    //    color += IndirectLighting(payload.rndseed, position, normal, V, metalColor.r,
	//			         surfaceColor, f0, roughness, payload.rayDepth);
    //}
    
    payload.color = color;
    payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(normal);
    payload.diffuseColor = surfaceColor;
    
    if (surfaceColor.a < 1.0f)
    {
        //float dist = 40.;
        //
        //payload.color *= surfaceColor.a;
        HitInfo reflPayload;
        reflPayload.color = float3(0, 0, 0);
        reflPayload.rayDepth = 0.0;
        reflPayload.normal = float3(0, 0, 0);
        ////Check if triangle is front or back facing
        //float3 refractedRay = float3(0,0,0);
        //float3 absorbColor = (1,1,1);
        //float3 objectAbsorb = (8.0, 2.0, 0.1);
        //
        ////leave the medium
        ////if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
        ////{
        ////    normal *= -1;
        ////    refractedRay = refract(WorldRayDirection(), normal, 1.0f/1.2f);
        ////    float distance = RayTCurrent();
        ////    absorbColor = exp(-objectAbsorb * distance);
        ////
        ////    payload.color = absorbColor;
        ////
        ////}
        //
        ////enter the medium
        //if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        //{
        //    refractedRay = refract(WorldRayDirection(), normal, 1.2);
        //    
        //    float3 color = float3(0, 0, 0);
        //    float T = 1.0f;
        //    float density = 0.f;
        //    
        //    float3 rayPoint = payload.currentPosition;
        //    float3 dir = WorldRayDirection();
        //    float Extinction = 1.0; // We start with full transparency
        //    float3 Scattering = float3(0, 0, 0); // We start with no accumulated lig
        //    for (int i = 0; i < 80; i++)
        //    {
        //        density += scene(rayPoint)/200.f;
        //    
        //        if (density > 0.)
        //        {
        //            float ScatteringCoeff = 5 * density;
        //            float ExtinctionCoeff = 5 * density;
        //    
        //        // Accumulate extinction for that step
        //            Extinction *= exp(-ExtinctionCoeff * (1.0f / 80.f));
        //            float3 StepScattering = ScatteringCoeff * 1.0f / 80.f;
        //            Scattering += Extinction * StepScattering; // Accumulate scattering attenuated by extinction        }
        //        }
        //    
        //        rayPoint += (1.0f / 100.f) * WorldRayDirection();
        //    }
        //    
        //    float4 finalColor = float4(1,0,0, density);
        //    
        //    RayDesc ray;
        //    ray.Origin = rayPoint;
        //    ray.Direction = dir;
        //    ray.TMin = 0.01;
        //    ray.TMax = 100000;
        ////
        //TraceRay(SceneBVH, RAY_FLAG_NONE, RAYTRACING_INSTANCE_OPAQUE | RAYTRACING_INSTANCE_TRANSCLUCENT, 0, 2, 0, ray, reflPayload);
        //    payload.color = reflPayload.color.rgb * (1. - finalColor.a) + reflPayload.color.rgb * finalColor.a;
        //
        //}
        //
        ////Totol internal reflection
        //if (refractedRay.x == 0 && refractedRay.y == 0 && refractedRay.z == 0)
        //{
        //            //total internal reflection
        //    refractedRay = reflect(WorldRayDirection(), refractedRay);
        //}
        //
 
        //RayDesc ray;
        //ray.Origin = payload.currentPosition;
        //ray.Direction = refractedRay;
        //ray.TMin = 0.01;
        //ray.TMax = 100000;
        //
        //TraceRay(SceneBVH, RAY_FLAG_NONE, RAYTRACING_INSTANCE_OPAQUE | RAYTRACING_INSTANCE_TRANSCLUCENT, 0, 2, 0, ray, reflPayload);
        
        //{
        //    float3 rayPoint = payload.currentPosition;
        //    float3 dir = WorldRayDirection();
        //    for (int i = 0; i < 70;i++)
        //    {
        //        float res =  min
        //    (
        //        sdSphere(rayPoint - float3(1.5, 0, 0), 2), // Left sphere
        //        sdSphere(rayPoint + float3(1.5, 0, 0), 2) // Right sphere
        //    );
        //        rayPoint += res * WorldRayDirection();
        //
        //    }
        //    payload.color = scene(rayPoint).rrr;
        //
        //}
        
        {
            RayDesc opaqueRay;
            opaqueRay.Origin = WorldRayOrigin();
            opaqueRay.Direction = WorldRayDirection();
            opaqueRay.TMin = 0.01;
            opaqueRay.TMax = 100000;
            
            HitInfo opaquePayload;

            //finalColor.rgb *= color;
            TraceRay(SceneBVH, RAY_FLAG_NONE, RAYTRACING_INSTANCE_OPAQUE, 0, 2, 0, opaqueRay, opaquePayload);
            float3 curPos = payload.currentPosition;
            float3 volNorm;
            
            float volumeDepth = RaymarchHit(curPos, WorldRayDirection(), volNorm);
            float opaqueVisiblity = 1.0f;
            const float marchSize = 0.6f;
            float3 volumetricColor = float3(0, 0, 0);
            float3 volumeAlbedo = float3(0.8, 0.8, 0.8);
            float3 alpha = 0;
            if (volumeDepth > 0.0)
            {
                alpha = 1;
                for (int i = 0; i <25; i++)
                {
                    volumeDepth += marchSize*1.8;
	                 
                    if (volumeDepth > opaquePayload.tCurrent)
                        break;
                    
                    float3 position = payload.currentPosition + volumeDepth * WorldRayDirection() * ((i+InterleavedGradientNoise(DispatchRaysIndex().xy, 1)) / 25.0f);
                    bool isInVolume = QueryVolumetricDistanceField(position) < 0.0f;
                    if (isInVolume)
                    {
                        float previousOpaqueVisiblity = opaqueVisiblity;
                        opaqueVisiblity *= BeerLambert(0.5 * GetFogDensity(position, QueryVolumetricDistanceField(position)), marchSize);
                        float absorptionFromMarch = previousOpaqueVisiblity - opaqueVisiblity;
                        float3 lightColor = float3(0, 0, 0);
                        for (int lightIndex = 0; lightIndex < 3; lightIndex++)
                        {
                            Light light = lights[lightIndex];
                            float3 L = float3(0, 0, 0);
                            float lightDistance = 0;
                            if (light.type == LIGHT_TYPE_DIR)
                            {
                                L = -light.direction;
                               
                                L = normalize(L);
                                
                                lightDistance = 100000;
                                
                                lightColor = light.color*light.intensity;
;
                            }
                            else if (light.type == LIGHT_TYPE_POINT)
                            {
                                L = (light.position - position);
                                float atten = Attenuate(light, position);
                                lightDistance = length(L);
                                
                                L = L / max(0.001, lightDistance);
                                
                                
                                lightColor = light.color * atten * light.intensity;
                            }
                            else if (light.type == LIGHT_TYPE_SPOT)
                            {
                                L = (light.position - position);
                                float atten = Attenuate(light, position);

                                lightDistance = length(L);
                                
                                L = L / max(lightDistance, 0.001);
                                
                                lightColor =  light.color * atten * light.intensity;
                            }
                            float visibility = GetLightVisibility(position, L, lightDistance, 4, 0.65*1.8);
                            volumetricColor += volumeAlbedo * lightColor * absorptionFromMarch*visibility;
                        }
                    }
                }
            }
            float4 finalColor;; //lerp(float4(0, 0, 0, 0), float4(0.5, 0.5, 0.5, 1), RaymarchHit(curPos, WorldRayDirection(), volNorm));
            RayDesc ray;
            ray.Origin = curPos;
            ray.Direction = WorldRayDirection();
            ray.TMin = 0.01;
            ray.TMax = 100000;
            
            
            
            //finalColor.rgb *= color;
            TraceRay(SceneBVH, RAY_FLAG_NONE, RAYTRACING_INSTANCE_OPAQUE | RAYTRACING_INSTANCE_TRANSCLUCENT, 0, 2, 0, ray, reflPayload);
            payload.color = reflPayload.color.rgb * (1. - alpha) + volumetricColor.rgb;

        }
    }
        
    
    else
    {
        //payload.color += color;
    }
    //payload.color = color;

    
}



[shader("closesthit")]
void PlaneClosestHit(inout HitInfo payload, Attributes attrib)
{

}
