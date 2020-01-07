cbuffer externalData: register(b0)
{
	matrix view;
	matrix projection;
	matrix model;
}

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
	output.position = float4(input.position,1.0);
	output.color = input.color;
	return output;
}