#include "Lighting.hlsli"

//cbuffer LightData: register(b1)
//{
//	DirectionalLight light1;
//	float3 cameraPosition;
//};

cbuffer LightingData : register(b1)
{
	float3 cameraPosition;
	uint lightCount;
};

struct Index
{
	uint index;
};

ConstantBuffer<Index> entityIndex: register(b0);
StructuredBuffer<uint> LightIndices : register(t1, space2);

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

float4 main(VertexToPixel input) : SV_TARGET
{
    uint2 location = uint2(input.position.xy);
    uint2 tileID = location / uint2(16, 16);
    uint numberOfTilesX = 1280 / 16;
    uint tileIndex = tileID.y * numberOfTilesX + tileID.x;

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
	
    uint offset = tileIndex * 1024;

	
    float3 Lo = float3(0, 0, 0);
	
	if(true)
    {
			//[loop]
		for (uint i = 0; i < 1024 && LightIndices[offset + i] != -1; i++)
		{
		    uint lightIndex = LightIndices[offset+i];
				
		    switch (lights[lightIndex].type)
		    {
			case LIGHT_TYPE_DIR:
                    Lo += DirectionLight(lights[lightIndex], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
				break;
			case LIGHT_TYPE_SPOT:
                    Lo += SpotLight(lights[lightIndex], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
				break;
			case LIGHT_TYPE_POINT:
                    Lo += PointLight(lights[lightIndex], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
						break;
		    }
		}
    }
    else
    {
    
        for (int i = 0; i < (int) lightCount; i++)
        {
            switch (lights[i].type)
            {
                case LIGHT_TYPE_DIR:
                    Lo += DirectionLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
                    break;
                case LIGHT_TYPE_SPOT:
                    Lo += SpotLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
                    break;
                case LIGHT_TYPE_POINT:
                    Lo += PointLight(lights[i], N, input.worldPosition, cameraPosition, 64, texColor.xyz);
                    break;
            }
        }
    }

    return float4(Lo, texColor.w);
}