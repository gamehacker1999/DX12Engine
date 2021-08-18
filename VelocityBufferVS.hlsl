#include "Common.hlsl"
struct CameraConstantBuffer
{
	matrix view;
	matrix projection;
	matrix world;
	matrix prevView;
	matrix prevProjection;
	matrix prevWorld;
};

ConstantBuffer<CameraConstantBuffer> sceneData : register(b0);


struct VertexShaderInput
{
	float3 position: POSITION;
	float3 normal: NORMAL;
	float3 tangent: TANGENT;
	float2 uv: TEXCOORD;

};

struct VertexToPixel
{
	float4 position: SV_POSITION;
    float4 curPosition : POSITION;
	float4 prevPosition: POSITION1;

};

VertexToPixel main(VertexShaderInput input)
{

	VertexToPixel output;

	matrix worldViewProj = mul(sceneData.world, mul(sceneData.view, sceneData.projection));
	matrix prevWorldViewProj = mul(sceneData.prevWorld, mul(sceneData.prevView, sceneData.prevProjection));

	output.position = mul(float4(input.position, 1.0), worldViewProj);
    output.curPosition = mul(float4(input.position, 1.0), worldViewProj);
	output.prevPosition = mul(float4(input.position, 1.0f), prevWorldViewProj);
	return output;
}