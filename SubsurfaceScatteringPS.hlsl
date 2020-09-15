#include"SphericalGaussian.hlsli"
#include "Lighting.hlsli"


cbuffer LightingData : register(b1)
{
    float3 cameraPosition;
    uint lightCount;
};

struct Index
{
	uint index;
};

struct SubsurfaceScattering
{
	bool enableSSS;
};
ConstantBuffer<SubsurfaceScattering> subsurfaceScattering : register(b2);

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

inline float3 UnpackNormal(float3 packednormal)
{
    float3 normal;
    normal.xy = packednormal.xy * 2 - 1;
    normal.z = sqrt(1 - normal.x * normal.x - normal.y * normal.y);
    return normal;
}

//function that accepts light and normal and then calculates the final color
float3 CalculateLight(Light light, VertexToPixel input, Texture2D normalMap, SamplerState basicSampler)
{

	float3 normalVal = normalMap.Sample(basicSampler, input.uv).xyz;

    float3 unpackedNormal = UnpackNormal(normalVal);

	float3 normalValR = normalMap.SampleBias(basicSampler, input.uv,3).xyz;

    float3 unpackedNormalR = UnpackNormal(normalValR);

	float3 normalValG = normalMap.SampleBias(basicSampler, input.uv,2).xyz;

    float3 unpackedNormalG = UnpackNormal(normalValG);

	float3 normalValB = normalMap.SampleBias(basicSampler, input.uv,1).xyz;

    float3 unpackedNormalB = UnpackNormal(normalValB);


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
	
    float rcpDistFromCamera = 1 / distance(input.worldPosition, cameraPosition);
	
	  // Compute curvature
    float3 dx = ddx(N);
    float3 dy = ddy(N);
    float3 xneg = N - dx;
    float3 xpos = N + dx;
    float3 yneg = N - dy;
    float3 ypos = N + dy;
    float depth = length(input.worldPosition);
    float curvature = (cross(xneg, xpos).y - cross(yneg, ypos).x) * 4.0 / depth;

    float radius = clamp(length(fwidth(N)), 0, 1) / (length(fwidth(input.worldPosition)) * rcpDistFromCamera);

	//calculate the cosine of the angle to calculate specularity
	//I am calculating the light based on the phong reflection model
	float cosine = dot(R, V);
	cosine = saturate(cosine);
	float shininess = 128.f;
	float specularAmount = pow(cosine, shininess); //increase the cosine curve fall off

	float NdotL = dot(N, L);
	NdotL = saturate(NdotL); //this is the light amount, we need to clamp it to 0 and 1.0

	bool enableSSS = subsurfaceScattering.enableSSS;

	float3 diffuse = NdotL;
	if (enableSSS)
	{
        float c1 = clamp(length(fwidth(finalNormalR)), 0, 1) / length(fwidth(input.worldPosition));
        float c2 = clamp(length(fwidth(finalNormalG)), 0, 1) / length(fwidth(input.worldPosition));
        float c3 = clamp(length(fwidth(finalNormalB)), 0, 1) / length(fwidth(input.worldPosition));
		float3 scatterAmount = float3(1.f, 0.15f, 0.1f);
        float3 lambda = 1 / scatterAmount;
		//float3 radius = float3(c1, c2, c3);
		lambda *= radius*radius;
		SphericalGaussian redKernal = MakeNormalizedSG(L, lambda.x);
		SphericalGaussian greenKernal = MakeNormalizedSG(L, lambda.y);
		SphericalGaussian blueKernal = MakeNormalizedSG(L, lambda.z);

		//convolving the light source with the spherical gaussian kernels 
		diffuse = float3(SphericalGaussianIrradianceFitted(redKernal, finalNormalR).x, SphericalGaussianIrradianceFitted(greenKernal, finalNormalG).x, SphericalGaussianIrradianceFitted(blueKernal, finalNormalB).x);

	}

	float3 finalLight = light.color*diffuse;
	finalLight += specularAmount;

    return float3(finalLight)*light.intensity * 2;
}

Texture2D material[]: register(t0);

float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 texColor = material[index].Sample(basicSampler,input.uv);

    //return float4(1, 1, 1, 1);
	return float4(CalculateLight(lights[0], input, material[index + 1], basicSampler), 1.0f) * texColor;
}