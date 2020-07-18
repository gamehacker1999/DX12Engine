#include"SphericalGaussian.hlsli"

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

struct Index
{
	uint index;
};

struct SubsurfaceScattering
{
	bool enableSSS;
};

ConstantBuffer<Index> entityIndex: register(b0);
ConstantBuffer<SubsurfaceScattering> subsurfaceScattering: register(b2);

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
float4 CalculateLight(DirectionalLight light, VertexToPixel input, Texture2D normalMap, SamplerState basicSampler)
{

	float3 normalVal = normalMap.Sample(basicSampler, input.uv).xyz;

	float3 unpackedNormal = normalVal * 2.0 - 1.0f;

	float3 normalValR = normalMap.SampleBias(basicSampler, input.uv,3).xyz;

	float3 unpackedNormalR = normalValR * 2.0 - 1.0f;

	float3 normalValG = normalMap.SampleBias(basicSampler, input.uv,2).xyz;

	float3 unpackedNormalG = normalValG * 2.0 - 1.0f;

	float3 normalValB = normalMap.SampleBias(basicSampler, input.uv,1).xyz;

	float3 unpackedNormalB = normalValB * 2.0 - 1.0f;


	float3 N = normalize(input.normal);
	float3 T = input.tangent - dot(input.tangent, N) * N;
	T = normalize(T);
	float3 B = normalize(cross(T, N));

	float3x3 TBN = float3x3(T, B, N);

	float3 finalNormal = mul(unpackedNormal, TBN);
	float3 finalNormalR = normalize(mul(unpackedNormalR, TBN));
	float3 finalNormalG = normalize(mul(unpackedNormalG, TBN));
	float3 finalNormalB = normalize(mul(unpackedNormalB, TBN));


	//standard N dot L calculation for the light
	float3 L = -light.direction;
	L = normalize(L); //normalizing the negated direction
	N = finalNormal;
	N = normalize(N); //normalizing the normal
	float3 R = reflect(-L, N); //reflect R over N
	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float4 NdotV = saturate(dot(N, V));
	float4 rimColor = float4(0.0f, 0.0f, 1.0f, 1.0f);

	float radius = length(fwidth(normalize(N))) / length(fwidth(input.worldPosition));

	//calculate the cosine of the angle to calculate specularity
	//I am calculating the light based on the phong reflection model
	float cosine = dot(R, V);
	cosine = saturate(cosine);
	float shininess = 8.f;
	float specularAmount = pow(cosine, shininess); //increase the cosine curve fall off

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

	bool enableSSS = subsurfaceScattering.enableSSS;

	float3 diffuse = NdotL;
	if (enableSSS)
	{
		float c1 = length(fwidth(finalNormalR)) / length(fwidth(input.worldPosition));
		float c2 = length(fwidth(finalNormalG)) / length(fwidth(input.worldPosition));
		float c3 = length(fwidth(finalNormalB)) / length(fwidth(input.worldPosition));
		float3 scatterAmount = float3(1.0f, 1.0f, 1.0f);
		//float3 radius = float3(c1, c2, c3);
		scatterAmount /= radius*radius;
		SphericalGaussian redKernal = MakeNormalizedSG(L, 1.0f/ (scatterAmount.x));
		SphericalGaussian greenKernal = MakeNormalizedSG(L, 1.0f/ (scatterAmount.y));
		SphericalGaussian blueKernal = MakeNormalizedSG(L, 1.0f/ (scatterAmount.z));

		//convolving the light source with the spherical gaussian kernels 
		diffuse = float3(SphericalGaussianIrradianceFitted(redKernal, finalNormalR).x, SphericalGaussianIrradianceFitted(greenKernal, finalNormalG).x, SphericalGaussianIrradianceFitted(blueKernal, finalNormalB).x);

		diffuse = saturate(diffuse);

	}

	//float4 diffuse = celShading.Sample(basicSampler, NdotL);
	//return diffuse;

	//adding diffuse, ambient, and specular color
	float4 finalLight = light.diffuse * float4(diffuse,1.0);
	finalLight += specularAmount;

	return finalLight*2;
}

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 texColor = material[index].Sample(basicSampler,input.uv);

    return float4(1, 1, 1, 1);
	return float4(CalculateLight(light1, input, material[index + 1], basicSampler)) * texColor * 1/M_PI;
}