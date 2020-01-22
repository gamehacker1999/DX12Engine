cbuffer SceneConstantBuffer: register(b0)
{
	matrix view;
	matrix projection;
	matrix world;
};

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
};

VertexToPixel main(VertexShaderInput input)
{
	VertexToPixel output;
	
	matrix worldViewProj = mul(world,mul(view, projection));
	output.position = mul(float4(input.position,1.0), worldViewProj);
	output.color = float4(input.normal,1);
	return output;
}