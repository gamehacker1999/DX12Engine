struct VertexToPixel
{
	float4 position:  SV_POSITION;
	float2 uv:		  TEXCOORD;
};

Texture2D hdrTarget: register(t0);
SamplerState basicSampler: register(s0);

float3 uncharted2_tonemap_partial(float3 x)
{
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 uncharted2_filmic(float3 v)
{
	float exposure_bias = 2.0f;
	float3 curr = uncharted2_tonemap_partial(v * exposure_bias);

	float3 W = float3(11.2f, 11.2f, 11.2f);
	float3 white_scale = float3(1.0f, 1.0f, 1.0f) / uncharted2_tonemap_partial(W);
	return curr * white_scale;
}

float4 main(VertexToPixel input) : SV_TARGET
{
	float gamma = 2.2;
	float3 hdrColor = hdrTarget.Sample(basicSampler, input.uv).rgb;
    // reinhard tone mapping
	float3 mapped = hdrColor / (hdrColor + float3(1.0, 1.0, 1.0));
	// gamma correction 

	float exposureBias = 2.0f;
	float3 curr = uncharted2_tonemap_partial(hdrColor);

	float gammaFactor = 1.0 / gamma;
	mapped = pow(mapped, gammaFactor.xxx);

	float3 finalCol = uncharted2_filmic(hdrColor);
	
	return float4(finalCol, 1.0);
}