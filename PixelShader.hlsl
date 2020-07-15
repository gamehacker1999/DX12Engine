#include "Lighting.hlsli"

//cbuffer LightData: register(b1)
//{
//	DirectionalLight light1;
//	float3 cameraPosition;
//};

cbuffer LightingData : register(b1)
{
	Light lights[MAX_LIGHTS];
	float3 cameraPosition;
	uint lightCount;
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

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 texColor = material[index].Sample(basicSampler,input.uv);

	float3 normal = material[index + 1].Sample(basicSampler, input.uv).xyz;

	float3 unpackedNormal = normal * 2.0 - 1.0f;

	float3 N = normalize(input.normal);
	float3 T = input.tangent - dot(input.tangent, N) * N;
	T = normalize(T);
	float3 B = normalize(cross(T, N));

	float3x3 TBN = float3x3(T, B, N);

	float3 finalNormal = mul(unpackedNormal, TBN);
    N = normalize(finalNormal);
	
    float3 Lo = float3(0, 0, 0);
	for (int i = 0; i < lightCount; i++)
	{
		switch (lights[i].type)
		{
			case LIGHT_TYPE_DIR:
                Lo += DirectionalLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
				break;
			case LIGHT_TYPE_SPOT:
				Lo += SpotLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
				break;
			case LIGHT_TYPE_POINT:
				Lo += PointLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
				break;
		}
	}

    return float4(Lo, texColor.w);
}