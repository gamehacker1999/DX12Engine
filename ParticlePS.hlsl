struct VertexToPixel
{
	float4 position: SV_POSITION;
	float2 uv : TEXCOORD;
	float4 color: COLOR;
};

Texture2D particle: register(t0);
SamplerState sampleOptions: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	float4 color = particle.Sample(sampleOptions,input.uv) * input.color;

	return color;
}