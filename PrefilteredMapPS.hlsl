#include "Utils.hlsli"

//struct to represent the vertex shader input
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
};

struct RoughnessData
{
	float roughness;
};

cbuffer FaceIndex: register(b1)
{
	int faceIndex;
};

ConstantBuffer<RoughnessData> roughnessData: register(b0);

Texture2D skybox : register(t0);
SamplerState basicSampler: register(s0);
//function to generate the van der corpus sequence
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
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

//function to generate a hammersly low discrepency sequence for importance sampling
float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

//importance sampling
float3 ImportanceSamplingGGX(float2 xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * PI * xi.x;
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

float rand(float3 value, float mutator = 0.546)
{
    float random = frac(sin(value + mutator) * 143758.5453);
    return random;
}

float4 main(VertexToPixel input) : SV_TARGET
{

	// Get a -1 to 1 range on x/y
	float2 o = input.uv * 2 - 1;
	
	// Tangent basis
	float3 xDir, yDir, zDir;
	
	// Figure out the z ("normal" of this pixel)
	switch (faceIndex)
	{
	default:
	case 0: zDir = float3(+1, -o.y, -o.x); break;
	case 1: zDir = float3(-1, -o.y, +o.x); break;
	case 2: zDir = float3(+o.x, +1, +o.y); break;
	case 3: zDir = float3(+o.x, -1, -o.y); break;
	case 4: zDir = float3(+o.x, -o.y, +1); break;
	case 5: zDir = float3(-o.x, -o.y, -1); break;
	}
	zDir = normalize(zDir);

	float3 N = normalize(zDir);
	float3 R = N;
	float3 V = R;

	uint sampleCount = 6000;
	float totalWeight = 0.0;
	float3 prefilteredColor = float3(0.0, 0.0, 0.0);
	
    float gr = (sqrt(5.0) + 1.0) / 2.0; // golden ratio = 1.6180339887498948482
    float ga = (2.0 - gr) * (2.0 * 3.151592); // golden angle = 2.39996322972865332
	
	for (uint i = 0; i < sampleCount; i++)
	{
		
        float2 xi2 = float2(rand(zDir, i), rand(zDir, i));
		
		float2 xi = Hammersley(i, sampleCount);
		float3 H = ImportanceSamplingGGX(xi2, N, roughnessData.roughness);
		float3 L = normalize(2 * dot(V, H) * H - V);

		float NdotL = saturate(dot(N, L));


		float2 uv = DirectionToLatLongUV(L);

		prefilteredColor += skybox.Sample(basicSampler, uv).rgb * NdotL;

		totalWeight += NdotL;
	}

	prefilteredColor /= totalWeight;

	return float4(prefilteredColor, 1.0f);

}