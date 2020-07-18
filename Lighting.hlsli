
#ifndef __LIGHTING_HLSLI__
#define __LIGHTING_HLSLI__
#include "LTCLighting.hlsli"

#define LIGHT_TYPE_DIR 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_AREA_RECT 3

static const float PI = 3.14159265359f;


float Attenuate(Light light, float3 worldPos)
{
    float dist = distance(light.position, worldPos);

    float att = saturate(1.0f - (dist * dist / (light.range * light.range)));

    return att * att;
}

// Lambert diffuse BRDF
float Diffuse(float3 normal, float3 dirToLight)
{
    return saturate(dot(normal, dirToLight));
}

// Blinn-Phong (specular) BRDF
float SpecularBlinnPhong(float3 normal, float3 dirToLight, float3 toCamera, float shininess)
{
	// Calculate halfway vector
    float3 halfwayVector = normalize(dirToLight + toCamera);

	// Compare halflway vector and normal and raise to a power
    return shininess == 0 ? 0.0f : pow(max(dot(halfwayVector, normal), 0), shininess);
}

float CalculateDiffuse(float3 n, float3 l)
{
	float3 L = l;
	L = normalize(L); //normalizing the negated direction
	float3 N = n;
	N = normalize(N); //normalizing the normal

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

    return NdotL;
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
	denom *= PI;

	return a2 / denom;

}

//function that calculates the cook torrence brdf
void CookTorrence(float3 n, float3 h, float roughness, float3 v, float3 f0, float3 l, out float3 F, out float D, out float G)
{
	D = SpecularDistribution(roughness, h, n);
	F = Fresnel(h, v, f0);
	G = GeometricShadowing(n, v, h, roughness) * GeometricShadowing(n, l, h, roughness);

}


float3 DirectionalLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Get normalize direction to the light
    float3 toLight = normalize(-light.direction);
    float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
    float diff = Diffuse(normal, toLight);
    float spec = SpecularBlinnPhong(normal, toLight, toCam, shininess);

	// Combine
    return (diff * surfaceColor + spec) * light.intensity * light.color;
}


float3 PointLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Calc light direction
    float3 toLight = normalize(light.position - worldPos);
    float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
    float atten = Attenuate(light, worldPos);
    float diff = Diffuse(normal, toLight);
    float spec = SpecularBlinnPhong(normal, toLight, toCam, shininess);

	// Combine
    return (diff * surfaceColor + spec) * atten * light.intensity * light.color;
}


float3 SpotLight(Light light, float3 normal, float3 worldPos, float3 camPos, float shininess, float3 surfaceColor)
{
	// Calculate the spot falloff
    float3 toLight = normalize(light.position - worldPos);
    float penumbra = pow(saturate(dot(-toLight, light.direction)), light.spotFalloff);
	
	// Combine with the point light calculation
	// Note: This could be optimized a bit
    return PointLight(light, normal, worldPos, camPos, shininess, surfaceColor) * penumbra;
}



float3 DirectLightPBR(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	//variables for different functions
	float3 F; //fresnel
	float D; //ggx
	float G; //geomteric shadowing

	float3 V = normalize(cameraPos - worldPos);
	float3 L = -light.direction;
	float3 H = normalize(L + V);
	float3 N = normalize(normal);

	CookTorrence(normal, H, roughness, V, f0, L, F, D, G);

	float3 ks = F;
	float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
	kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

	float lambert = CalculateDiffuse(normal, L);
	float3 numSpec = D * F * G;
	float denomSpec = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert*light.intensity;
}

float3 PointLightPBR(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
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

	CookTorrence(normal, H, roughness, V, f0, L, F, D, G);

	float lambert = CalculateDiffuse(normal, L);
	float3 ks = F;
	float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
	kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

	float3 numSpec = D * F * G;
	float denomSpec = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert * atten *light.intensity;

}

float3 SpotLightPBR(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	float3 L = normalize(light.position - worldPos);
	float3 penumbra = pow(saturate(dot(-L, light.direction)), light.spotFalloff);

	return PointLightPBR(light, normal, worldPos, cameraPos, roughness, metalness, surfaceColor, f0) * penumbra;
}

float3 RectAreaLightPBR(Light light, float3 normal, float3 view ,float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0, 
                        float4 ltc1, float4 ltc2, Texture2D ltcTex, SamplerState samplerState)
{
	
    Rect rect;
    InitRect(rect, light);
    
    float3 points[4];
    InitRectPoints(rect, points);
	
	//calculating the inverse matrix to transform the distribution back into the clamped cosine
    float3x3 minV = float3x3(
	   float3(1, 0, ltc1.y),
	   float3(0, ltc1.z, 0),
	   float3(ltc1.w, 0, ltc1.x)
	);
	
    minV = transpose(minV);
    
    float3 spec = LTC_Evaluate(normal, view, worldPos, minV, points, ltc2,false, ltcTex, samplerState);
    
    spec *= f0 * (ltc2.x) + (1 - f0) * (ltc2.y);
    
    float3 diffuse = LTC_Evaluate(normal, view, worldPos, float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1), points, ltc2,false, ltcTex, samplerState);
    float3 color = light.intensity * (light.color) * (spec + diffuse*surfaceColor);

    return color / (2*PI);

}

#endif