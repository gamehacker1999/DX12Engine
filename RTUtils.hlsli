#include "Common.hlsl"
#include "Lighting.hlsli"

#define NUM_SAMPLES 1

static const float M_PI = 3.14159265f;

struct Vertex
{
    float3 Position; // The position of the vertex
    float3 Normal;
    float3 Tangent;
    float2 UV;
};

StructuredBuffer<Vertex> vertex : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D material[] : register(t0, space1);

struct Index
{
    uint index;
};

ConstantBuffer<Index> entityIndex : register(b0);

cbuffer LightingData : register(b1)
{
    float3 cameraPosition;
    uint lightCount;
};


float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

//function to generate a hammersly low discrepency sequence for importance sampling
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

//importance sampling
float3 ImportanceSamplingGGX(float2 xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * M_PI * xi.x;
    float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.f) * xi.y));
    float sinTheta = 1 - (cosTheta * cosTheta); //using the trigonometric rule

	//from spherica coordinats to cartsian space
    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

	//from tangent to world space using TBN matrix
    float3 up = abs(N.z) < 0.999 ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f); //up vector is based on the value of N
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + H.z * N;

    return normalize(sampleVec);
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// A work-around function because some DXR drivers seem to have broken atan2() implementations
float atan2_WAR(float y, float x)
{
	if (x > 0.f)
		return atan(y / x);
	else if (x < 0.f && y >= 0.f)
		return atan(y / x) + M_PI;
	else if (x < 0.f && y < 0.f)
		return atan(y / x) - M_PI;
	else if (x == 0.f && y > 0.f)
		return M_PI / 2.f;
	else if (x == 0.f && y < 0.f)
		return -M_PI / 2.f;
	return 0.f; // x==0 && y==0 (undefined)
}

// Convert our world space direction to a (u,v) coord in a latitude-longitude spherical map
float2 wsVectorToLatLong(float3 dir)
{
	float3 p = normalize(dir);

	// atan2_WAR is a work-around due to an apparent compiler bug in atan2
	float u = (1.f + atan2_WAR(p.x, -p.z) * M_PI) * 0.5f;
	float v = acos(p.y) * M_PI;
	return float2(u, v);
}

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 GetCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = getPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(max(0.0, 1.0f - randVal.x));
}

float3 ConvertFromObjectToWorld(float4 vec)
{
	return mul(vec, ObjectToWorld4x3()).xyz;
}

// Approximates luminance from an RGB value
float Luminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float probabilityToSampleDiffuse(float3 difColor, float3 specColor)
{
    float lumDiffuse = max(0.01f, Luminance(difColor.rgb));
    float lumSpecular = max(0.01f, Luminance(specColor.rgb));
    return lumDiffuse / (lumDiffuse + lumSpecular);
}

float ShootShadowRays(float3 origin, float3 direction, float minT, float maxT)
{
    //initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;
    ///shadowPayload.primitiveIndex = InstanceID();

    /**/
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = minT;
    ray.TMax = maxT;

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);

    return shadowPayload.isHit ? 0.3 : 1.0f;
}

//function that calculates the cook torrence brdf
void CookTorrenceRaytrace(float3 n, float3 h, float roughness, float3 v, float3 f0, float3 l, out float3 F, out float D, out float G)
{

    D = SpecularDistribution(roughness, h, n);
    F = Fresnel(h, v, f0);
    G = GeometricShadowing(n, v, h, roughness) * GeometricShadowing(n, l, h, roughness);

}


float3 DirectLightPBRRaytrace(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	//variables for different functions
    float3 F; //fresnel
    float D; //ggx
    float G; //geomteric shadowing

    float3 V = normalize(cameraPos - worldPos);
    float3 L = -light.direction;
    float3 H = normalize(L + V);
    float3 N = normalize(normal);

    CookTorrenceRaytrace(normal, H, roughness, V, f0, L, F, D, G);

    float3 ks = F;
    float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
    kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

    float lambert = CalculateDiffuse(normal, L);
    float3 numSpec = D * F * G;
    float denomSpec = 4.0f * max(dot(N, V), 0.001f) * max(dot(N, L), 0.001f);
    float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

    return ((kd * surfaceColor.xyz / PI) + specular) * lambert * light.intensity * light.color;
}

float3 PointLightPBRRaytrace(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	//variables for different functions
    float3 F; //fresnel
    float D; //ggx
    float G; //geomteric shadowing

	//light direction calculation
    float3 L = normalize(light.position - worldPos);
    float3 V = normalize(cameraPos - worldPos);
    float3 H = normalize(L + V);
    float3 N = normalize(normal);

    float atten = Attenuate(light, worldPos);

    CookTorrenceRaytrace(normal, H, roughness, V, f0, L, F, D, G);

    float lambert = CalculateDiffuse(normal, L);
    float3 ks = F;
    float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
    kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

    float3 numSpec = D * F * G;
    float denomSpec = 4.0f * max(dot(N, V), 0.001f) * max(dot(N, L), 0.001f);
    float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

    return ((kd * surfaceColor.xyz / PI) + specular) * lambert * atten * light.intensity * light.color;

}

float3 SpotLightPBRRaytrace(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
    float3 L = normalize(light.position - worldPos);
    float3 penumbra = pow(saturate(dot(-L, light.direction)), light.spotFalloff);

    return PointLightPBRRaytrace(light, normal, worldPos, cameraPos, roughness, metalness, surfaceColor, f0) * penumbra;
}


float3 DirectLighting(float rndseed, float3 pos, float3 norm, float3 V, float metalColor, float3 surfaceColor, float3 f0, float roughness)
{
    float3 color = float3(0, 0, 0);

    for (int i = 0; i < lightCount; i++)
    {
        if (lights[i].type == LIGHT_TYPE_DIR)
        {
            color += DirectLightPBRRaytrace(lights[i], norm, pos, cameraPosition, roughness, metalColor, surfaceColor, f0);
        }
        else if (lights[i].type == LIGHT_TYPE_POINT)
        {
            color += PointLightPBRRaytrace(lights[i], norm, pos, cameraPosition, roughness, metalColor, surfaceColor, f0);
        }
        else if (lights[i].type == LIGHT_TYPE_SPOT)
        {
            color += SpotLightPBRRaytrace(lights[i], norm, pos, cameraPosition, roughness, metalColor, surfaceColor, f0);
        }
    }
    

        
    return color * ShootShadowRays(pos + norm, -lights[0].direction, 0.001, 1000000);
}

float3 IndirectDiffuseLighting(inout float rndseed, float3 pos, float3 norm, float3 V, float metalColor, float3 surfaceColor, float3 f0, float roughness, float rayDepth)
{
    float probDiffuse = probabilityToSampleDiffuse(surfaceColor, f0);
    float chooseDiffuse = (nextRand(rndseed) < probDiffuse);
    float3 response = float3(0, 0, 0);
    
    if (true)
    {
	// Shoot a randomly selected cosine-sampled diffuse ray.
        
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            float3 L = GetCosHemisphereSample(rndseed, norm);
        
            HitInfo giPayload = { float3(0, 0, 0), rayDepth, rndseed, pos, norm, surfaceColor };
       
            RayDesc ray;
            ray.Origin = pos;
            ray.Direction = L;
            ray.TMin = 0.01;
            ray.TMax = 100000;
        
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
            
            response += giPayload.color * surfaceColor / max((probDiffuse), 0.0001f);

        }
        
        return response /= NUM_SAMPLES;
        
    }
}

float3 IndirectSpecularLighting(inout float rndseed, float3 pos, float3 norm, float3 V, float metalColor, float3 surfaceColor, float3 f0, float roughness, float rayDepth)
{
    
     float probDiffuse = probabilityToSampleDiffuse(surfaceColor, f0);
     float randNum = nextRand(rndseed);
     float chooseDiffuse = (randNum < probDiffuse);
     float3 response = float3(0, 0, 0);
     for (int i = 0; i < NUM_SAMPLES; i++)
     {
         float2 randVals = Hammersley(randNum* 4096, 4096);
         float3 H = ImportanceSamplingGGX(randVals, norm, roughness);
         float3 L = normalize(2 * dot(V, H) * H - V);
     
         HitInfo giPayload = { float3(0, 0, 0), rayDepth, rndseed, pos, norm, surfaceColor };
     
         RayDesc ray;
         ray.Origin = pos;
         ray.Direction = L;
         ray.TMin = 0.01;
         ray.TMax = 100000;
     
         TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
	
	 // Compute some dot products needed for shading
         float NdotL = saturate(dot(norm, L));
         float NdotH = saturate(dot(norm, H));
         float LdotH = saturate(dot(L, H));
         float NdotV = saturate(dot(norm, V));
     
	 // valuate our BRDF using a microfacet BRDF model
         float D = SpecularDistribution(roughness, H, norm);
         float G = GeometricShadowing(norm, V, H, roughness) * GeometricShadowing(norm, L, H, roughness);
         float3 F = FresnelRoughness(NdotV, f0, roughness);
         float3 ggxTerm = D * G * F / max((4 * NdotL * NdotV), 0.001f);
     
        float ggxProb = D * NdotH / max((4 * LdotH), 0.00001);
         
        response += NdotL * giPayload.color * ggxTerm / max((ggxProb), 0.0001f);

     }
     
     response /= NUM_SAMPLES;
     
     return response;

   
}

float3 IndirectLighting(inout float rndseed, float3 pos, float3 norm, float3 V, float metalColor, float3 surfaceColor, float3 f0, float roughness, float rayDepth)
{
    float probDiffuse = probabilityToSampleDiffuse(surfaceColor, f0);
    float chooseDiffuse = (nextRand(rndseed) < probDiffuse);
    float3 response = float3(0, 0, 0);
    
    if (chooseDiffuse)
    {
	// Shoot a randomly selected cosine-sampled diffuse ray.
        
        for (int i = 0; i < NUM_SAMPLES;i++)
        {
            float3 L = GetCosHemisphereSample(rndseed, norm);
        
            HitInfo giPayload = { float3(0, 0, 0), rayDepth, rndseed, pos, norm, surfaceColor };
       
            RayDesc ray;
            ray.Origin = pos;
            ray.Direction = L;
            ray.TMin = 0.01;
            ray.TMax = 100000;
        
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
            
            response += giPayload.color * surfaceColor / max((probDiffuse), 0.0001f);

        }
        
        return response /= NUM_SAMPLES;
        
    }
    else
    {
               
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            float2 randVals = Hammersley(1, 4096);
            float3 H = ImportanceSamplingGGX(randVals, norm, roughness);
            float3 L = normalize(2 * dot(V, H) * H - V);
        
            HitInfo giPayload = { float3(0, 0, 0), rayDepth, rndseed, pos, norm, surfaceColor };
       
            RayDesc ray;
            ray.Origin = pos;
            ray.Direction = L;
            ray.TMin = 0.01;
            ray.TMax = 100000;
       
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, giPayload);
		
	   // Compute some dot products needed for shading
            float NdotL = saturate(dot(norm, L));
            float NdotH = saturate(dot(norm, H));
            float LdotH = saturate(dot(L, H));
            float NdotV = saturate(dot(norm, V));
       
	    // valuate our BRDF using a microfacet BRDF model
            float D = SpecularDistribution(roughness, H, norm);
            float G = GeometricShadowing(norm, V, H, roughness) * GeometricShadowing(norm, L, H, roughness);
            float3 F = FresnelRoughness(NdotV, f0, roughness);
            float3 ggxTerm = D * G * F / (4 * NdotL * NdotV);
        
            float ggxProb = D * NdotH / (4 * LdotH);
            
            response += NdotL * giPayload.color * ggxTerm / max((ggxProb * (1.0f - probDiffuse)), 0.0001f);

        }
        
        response /= NUM_SAMPLES;
        
        return response;

    }
}

