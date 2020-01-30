struct DirectionalLight
{
	float4 ambientColor;
	float4 diffuse;
	float4 specularity;
	float3 direction;
};

static const float PI = 3.14159265f;

cbuffer LightData: register(b1)
{
	DirectionalLight light1;
	float3 cameraPosition;
};

struct Index
{
	uint index;
};

ConstantBuffer<Index> entityIndex: register(b0);

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};

//function that accepts light and normal and then calculates the final color
/*float4 CalculateLight(DirectionalLight light, float3 normal, VertexToPixel input)
{
	//standard N dot L calculation for the light
	float3 L = -light.direction;
	L = normalize(L); //normalizing the negated direction
	float3 N = normal;
	N = normalize(N); //normalizing the normal
	float3 R = reflect(-L, N); //reflect R over N
	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float4 NdotV = saturate(dot(N, V));
	float4 rimColor = float4(0.0f, 0.0f, 1.0f, 1.0f);

	//calculate the cosine of the angle to calculate specularity
	//I am calculating the light based on the phong reflection model
	float cosine = dot(R, V);
	cosine = saturate(cosine);
	float shininess = 8.f;
	float specularAmount = pow(cosine, shininess); //increase the cosine curve fall off

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

	//float4 diffuse = celShading.Sample(basicSampler, NdotL);
	//return diffuse;

	//adding diffuse, ambient, and specular color
	float4 finalLight = light.diffuse * NdotL;
	finalLight += specularAmount;

	return finalLight;
}*/

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


float3 DirectLightPBR(DirectionalLight light, float3 normal, float3 worldPos, float3 cameraPos, 
	float roughness, float metalness, float3 surfaceColor, float3 f0)
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
	float denomSpec = 4.0f * max(dot(N, V), 0.0001f) * max(dot(N, L), 0.0001f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert;
}

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 surfaceColor = material[index+0].Sample(basicSampler,input.uv);

	surfaceColor = pow(abs(surfaceColor), 2.2);

	//getting the normal texture
	float3 normalColor = material[index + 1].Sample(basicSampler, input.uv).xyz;
	float3 unpackedNormal = normalColor * 2.0f - 1.0f;

	//orthonormalizing T, B and N using the gram-schimdt process
	float3 N = normalize(input.normal);
	float3 T = input.tangent - dot(input.tangent, N) * N;
	T = normalize(T);
	float3 B = normalize(cross(T,N));

	float3x3 TBN = float3x3(T, B, N); //getting the tbn matrix

	//transforming normal from map to world space
	float3 finalNormal = mul(unpackedNormal, TBN);

	//getting the metalness of the pixel
	float3 metalColor = material[index + 3].Sample(basicSampler, input.uv).xyz;

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, surfaceColor.xyz, metalColor);

	//getting the roughness of pixel
	float roughness = material[index + 2].Sample(basicSampler, input.uv).x;

	//step 1 --- Solving the radiance integral for direct lighting, the integral is just the number of light sources
	// the solid angle on the hemisphere in infinitely small, so the wi is just a direction vector
	//for now radiance is just the color of the direction light, the diffuse part is lambertian*c/pi
	//specular is calculated by cook torrence, which ontains in ks term in is due to fresnel

	float3 L = -light1.direction;
	L = normalize(L); //normalizing the negated direction
	N = finalNormal;
	N = normalize(N); //normalizing the normal
	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float3 H = normalize(L + V);
	float3 R = reflect(-V, N); //reflect R over N

	float3 color = DirectLightPBR(light1 , N, input.worldPosition, cameraPosition,
		roughness, metalColor.r, surfaceColor.xyz, f0);

	color = pow(abs(color), 1.f / 2.2f);

	return float4(color, 1.0f);
}