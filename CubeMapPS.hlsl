
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 worldPos		: TEXCOORD;
};

//variables for the textures
TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	//sample the skybox color
	float4 skyboxColor = skyboxTexture.Sample(basicSampler,input.worldPos);

	//return the color
	return skyboxColor;
}