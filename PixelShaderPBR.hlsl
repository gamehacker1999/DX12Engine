#include "Lighting.hlsli"

struct DirectionalLight
{
	float4 ambientColor;
	float4 diffuse;
	float4 specularity;
	float3 direction;
};

cbuffer LightData: register(b1)
{
	DirectionalLight light1;
	float3 cameraPosition;
};

//cbuffer LightingData : register(b1)
//{
//	Light lights[MAX_LIGHTS];
//	float3 cameraPosition;
//  uint lightCount;
//};

struct Index
{
	uint index;
};

ConstantBuffer <Index>entityIndex: register(b0);

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
	float denomSpec = 4.0f * max(dot(N, V), 0.0000f) * max(dot(N, L), 0.0000f);
	float3 specular = numSpec / max(denomSpec, 0.0001f); //just in case denominator is zero

	return ((kd * surfaceColor.xyz / PI) + specular) * lambert;
}

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);
TextureCube irradianceMap: register(t0, space1);
TextureCube prefilteredMap: register(t1, space1);
Texture2D brdfLUT: register(t2, space1);


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
	
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	//for (int i = 0; i < lightCount; i++)
	//{
	//	switch (lights[i].type)
	//	{
	//		case LIGHT_TYPE_DIR:
	//			Lo += DirectLightPBR(lights[i], N, input.worldPosition, cameraPosition,
	//		roughness, metalColor.r, surfaceColor.xyz, f0);
	//			break;
	//		case LIGHT_TYPE_SPOT:
	//			Lo += SpotLightPBR(lights[i], N, input.worldPosition, cameraPosition,
	//		roughness, metalColor.r, surfaceColor.xyz, f0);
	//			break;
	//		case LIGHT_TYPE_POINT:
	//			Lo += PointLightPBR(lights[i], N, input.worldPosition, cameraPosition,
	//		roughness, metalColor.r, surfaceColor.xyz, f0);
	//			break;
	//	}
	//}

	float3 color = DirectLightPBR(light1 , N, input.worldPosition, cameraPosition,
		roughness, metalColor.r, surfaceColor.xyz, f0);

	float3 ksIndirect = FresnelRoughness(dot(N, V), f0, roughness);

	float3 kdIndirect = float3(1.0f, 1.0f, 1.0f) - ksIndirect;

	kdIndirect *= (1 - metalColor);

	kdIndirect *= surfaceColor.rgb / PI;

	float3 irradiance = irradianceMap.Sample(basicSampler, N).rgb;

	float3 diffuseIndirect = surfaceColor.rgb * irradiance;

	float3 prefilteredColor = prefilteredMap.SampleLevel(basicSampler, R, roughness * 4.0).rgb;

	float2 envBRDF = brdfLUT.Sample(basicSampler, float2(saturate(dot(N, V)), roughness)).rg;

	float3 specularIndirect = prefilteredColor * (ksIndirect * envBRDF.x + envBRDF.y);

	float3 ambientIndirect = (kdIndirect * diffuseIndirect + specularIndirect * surfaceColor.rgb); 

	color += ambientIndirect;

	color = pow(abs(color), 1.f / 2.2f);

	return float4(color, 1.0f);
}