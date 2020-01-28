/*cbuffer SceneConstantBuffer: register(b0)
{
	matrix view;
	matrix projection;
	matrix world;
};*/

struct SceneConstantBuffer
{
	matrix view;
	matrix projection;
	matrix world;
};

struct Index
{
	uint index;
};

ConstantBuffer<Index> entityIndex: register(b0);
ConstantBuffer<SceneConstantBuffer> sceneData[]: register(b0, space1);

struct VertexShaderInput
{
	float3 position: POSITION;
	float3 normal: NORMAL;
	float2 uv: TEXCOORD;

};

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};

VertexToPixel main(VertexShaderInput input)
{

	VertexToPixel output;

	matrix worldViewProj = mul(sceneData[entityIndex.index].world, mul(sceneData[entityIndex.index].view, sceneData[entityIndex.index].projection));
	output.position = mul(float4(input.position, 1.0), worldViewProj);
	output.normal = mul(input.normal, (float3x3)sceneData[entityIndex.index].world);
	output.uv = input.uv;
	output.color = float4(input.normal, 1);
	output.worldPosition = mul(float4(input.position, 1.0f), sceneData[entityIndex.index].world).xyz;
	return output;
}