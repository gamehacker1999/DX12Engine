cbuffer SceneConstantBuffer: register(b0)
{
	float4 offset;
	matrix view;
	matrix projection;
};

struct VertexShaderInput
{
	float3 position: POSITION;
	float4 color: COLOR;

};

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
};

VertexToPixel main(VertexShaderInput input)
{
	VertexToPixel output;
	
	matrix viewProj = mul(view, projection);
	output.position = mul(float4(input.position,1.0), viewProj);
	output.color = input.color;
	return output;
}