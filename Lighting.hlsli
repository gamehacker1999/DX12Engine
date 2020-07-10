
#ifndef __LIGHTING_HLSLI__
#define __LIGHTING_HLSLI__
#define LIGHT_TYPE_DIR 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_POINT 2

static const float PI = 3.14159265f;

#define MAX_LIGHTS 128

struct Light
{
	int type;
	float3 direction;
	float range;
	float3 position;
	float intensity;
	float3 diffuse;
	float spotFalloff;
	float3 padding;

};

float Attenuate(Light light, float3 worldPos)
{
	float dist = distance(light.position, worldPos);

	float att = saturate(1.0f - (dist * dist / (light.range * light.range)));

	return att * att;
}

float3 CalculateDiffuse(float3 n, float3 l, Light light)
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

	float3 lambert = CalculateDiffuse(normal, L, light);
	float3 numSpec = D * F * G;
	float denomSpec = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert;
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

	float3 lambert = CalculateDiffuse(normal, L, light);
	float3 ks = F;
	float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
	kd *= (float3(1.0f, 1.0f, 1.0f) - metalness);

	float3 numSpec = D * F * G;
	float denomSpec = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert * atten;

}

float3 SpotLightPBR(Light light, float3 normal, float3 worldPos, float3 cameraPos, float roughness, float metalness, float3 surfaceColor, float3 f0)
{
	float3 L = normalize(light.position - worldPos);
	float3 penumbra = pow(saturate(dot(-L, light.direction)), light.spotFalloff);

	return PointLightPBR(light, normal, worldPos, cameraPos, roughness, metalness, surfaceColor, f0) * penumbra;
}


#endif