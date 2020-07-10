#include "RTUtils.hlsli"

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0);
RWTexture2D< float4 > gRoughnessMetallic : register(u1);
RWTexture2D< float4 > gPosition : register(u2);
RWTexture2D< float4 > gNormal : register(u3);
RWTexture2D< float4 > gAlbedo : register(u4);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

struct RayTraceCameraData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
};

cbuffer LightData: register(b1)
{
    DirectionalLight light1;
    float3 cameraPosition;
};



ConstantBuffer<RayTraceCameraData> cameraData: register(b0);

float ShootShadowRays(float3 origin, float3 direction, float minT, float maxT)
{
    //initialize the hit payload
	ShadowHitInfo shadowPayload;
	shadowPayload.isHit = false;

    /**/
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = minT;
	ray.TMax = maxT;

	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);

	return shadowPayload.isHit ? 0.3 : 1.0f;
}

float3 CalculateDiffuse(float3 n, float3 l, DirectionalLight light)
{
	float3 L = l;
	L = normalize(L); //normalizing the negated direction
	float3 N = n;
	N = normalize(N); //normalizing the normal

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

	//adding diffuse, ambient color
	float3 finalLight = light.diffuse.xyz * NdotL;
	//finalLight += light.ambientColor;
	return finalLight;
}

//function for the fresnel term(Schlick approximation)
float3 Fresnel(float3 h, float3 v, float3 f0)
{
	//calculating v.h
	float VdotH = saturate(dot(v, h));
	//raising it to fifth power
	float VdotH5 = pow(1 - VdotH, 5);

	float3 finalValue = f0 + (1 - f0) * VdotH5;

	return finalValue;
}

//fresnel shchlick that takes the roughness into account
float3 FresnelRoughness(float NdotV, float3 f0, float roughness)
{
	float VdotH = saturate(NdotV);

	float VdotH5 = pow(1 - VdotH, 5);

	float3 finalValue = f0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0) - f0) * VdotH5;

	return finalValue;

}

//function for the Geometric shadowing
// k is remapped to a / 2 (a is roughness^2)
// roughness remapped to (r+1)/2
float GeometricShadowing(
	float3 n, float3 v, float3 h, float roughness)
{
	// End result of remapping:
	float k = pow(roughness + 1, 2) / 8.0f;
	float NdotV = saturate(dot(n, v));

	// Final value
	return NdotV / (NdotV * (1 - k) + k);
}


//function for the GGX normal distribution of microfacets
float SpecularDistribution(float roughness, float3 h, float3 n)
{
	//remapping the roughness
	float a = pow(roughness, 2);
	float a2 = a * a;

	float NdotHSquared = saturate(dot(n, h));
	NdotHSquared *= NdotHSquared;

	float denom = NdotHSquared * (a2 - 1) + 1;
	denom *= denom;
	denom *= 3.14159f;

	return a2 / denom;

}

//function that calculates the cook torrence brdf
void CookTorrence(float3 n, float3 h, float roughness, float3 v, float3 f0, float3 l, out float3 F, out float D, out float G)
{
	D = SpecularDistribution(roughness, h, n);
	F = Fresnel(h, v, f0);
	G = GeometricShadowing(n, v, h, roughness) * GeometricShadowing(n, l, h, roughness);

}


float3 DirectLightPBR(float3 normal, float3 worldPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	//variables for different functions
	float3 F; //fresnel
	float D; //ggx
	float G; //geomteric shadowing

	float3 V = normalize(cameraPosition - worldPos);
	float3 L = -light1.direction;
	float3 H = normalize(L + V);
	float3 N = normalize(normal);

	CookTorrence(normal, H, roughness, V, f0, L, F, D, G);

	float3 ks = F;
	float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
	kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

	float3 lambert = CalculateDiffuse(normal, L, light1);
	float3 numSpec = D * F * G;
	float denomSpec = 4.0f * max(dot(N, V), 0.0000f) * max(dot(N, L), 0.0000f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero
	float factor = ShootShadowRays(worldPos, L, 1.0e-4f, 10000000);
	return ((kd * surfaceColor.xyz / 3.14159f) + specular) * lambert*factor;
}

float3 DiffuseShade(float3 pos, float3 norm, float3 albedo, inout uint seed)
{
    float3 L = -light1.direction;

    float NdotL = saturate(dot(norm, L));

    float factor = ShootShadowRays(pos, L, 1.0e-4f, 10000000);

    float3 rayColor = light1.diffuse*factor;

    return (NdotL * rayColor * (albedo/M_PI));
}

[shader("raygeneration")] 
void RayGen() {
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

    uint rndseed = initRand(launchIndex.x, launchIndex.y);

    float3 pos = gPosition[launchIndex].xyz;
    float3 norm = gNormal[launchIndex].xyz;
    float3 albedo = gAlbedo[launchIndex].xyz;
    float3 metalColor = gRoughnessMetallic[launchIndex].rgb;
    float roughness = gRoughnessMetallic[launchIndex].a;
    
	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, albedo, metalColor);
   
    float3 color = float3(0, 0, 0);

    if (norm.x == 0 && norm.y == 0 && norm.z == 0)
    {
        color = albedo;
    }

    else
    {
		color = DirectLightPBR(norm, pos, roughness, metalColor.x, albedo, f0);

        float3 giVal = float3(0, 0, 0);
        //for (int i = 0; i < 32; i++)
        //{
            float3 giDir = getCosHemisphereSample(rndseed, norm);

            HitInfo giPayload = { float3(0,0,0),payload.rayDepth,rndseed,pos,norm,albedo };

            /**/RayDesc ray2;
            ray2.Origin = pos;
            ray2.Direction = giDir;
            ray2.TMin = 0.01;
            ray2.TMax = 100000;


            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray2, giPayload);

            giVal += giPayload.color;
            color += albedo * giPayload.color;
        //}
        //giVal = giVal / 32.f;
        //color += albedo * giVal;;
    } 

    gOutput[launchIndex] = float4(color, 1.f);
}
