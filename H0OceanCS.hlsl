cbuffer philipsSpectrum: register(b0)
{
	int fftRes;
	int L;
	float amp;
	float2 windDir;
	float windSpeed;
}

static const float PI = 3.1415926535897932384626433832795f;
static const float g = 9.81f;

RWTexture2D<float4> tildeH0: register(u0);
RWTexture2D<float4> tildeMinusH0: register(u1);

//textures for the real and imaginary gaussian numbers
Texture2D noiseR1: register(t0);
Texture2D noiseI1: register(t1);
Texture2D noiseR2: register(t2);
Texture2D noiseI2: register(t3);
SamplerState sampleOptions: register(s0);

float4 GaussRNG(uint3 id)
{
	float2 texCoord = float2(id.xy) / (float)fftRes;
	float noise1 = saturate(noiseR1.SampleLevel(sampleOptions, texCoord, 0).r + 0.001f);
	float noise2 = saturate(noiseI1.SampleLevel(sampleOptions, texCoord, 0).r + 0.001f);
	float noise3 = saturate(noiseR2.SampleLevel(sampleOptions, texCoord, 0).r + 0.001f);
	float noise4 = saturate(noiseI2.SampleLevel(sampleOptions, texCoord, 0).r + 0.001f);

	//using the box mueller method to generate random gaussian vals
	float u0 = 2 * PI * noise1;
	float v0 = sqrt(-2 * log(noise2));
	float u1 = 2 * PI * noise3;
	float v1 = sqrt(-2 * log(noise4));

	float4 rnd = float4(v0 * cos(u0), v0 * sin(u0), v1 * cos(u1), v1 * sin(u1));

	return rnd;
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	float2 x = float2(id.xy) - float(fftRes) / 2.0f;
	float2 k = float2(2 * PI * x.x / L, 2 * PI * x.y / L);
	float L_ = (windSpeed * windSpeed) / g;
	float mag = length(k);
	if (mag < 0.00001f) mag = 0.00001;
	float magSq = mag * mag;

	//calcualting the functions on which to perform the fourier transform

	//using sqrt of philip of k
	float h0k = clamp(sqrt((amp / (magSq * magSq)) *
		pow(dot(normalize(k), normalize(windDir)), 2.0)
		* exp(-(1.0 / (magSq * L_ * L_)))
		* exp(-magSq * pow(L / 2000.0f, 2.0))) / sqrt(2.0), -4000.0, 4000.0);

	//using sqrt of philip of -k
	float h0Minusk = clamp(sqrt((amp / (magSq * magSq)) *
		pow(dot(normalize(-k), normalize(windDir)), 2.0)
		* exp(-(1.0 / (magSq * L_ * L_)))
		* exp(-magSq * pow(L / 2000.0f, 2.0))) / sqrt(2.0), -4000.0, 4000.0);

	float4 gaussianRND = GaussRNG(id);

	tildeH0[id.xy] = float4(gaussianRND.x * h0k, gaussianRND.y * h0k, 0, 1);
	tildeMinusH0[id.xy] = float4(gaussianRND.z * h0Minusk, gaussianRND.w * h0Minusk, 0, 1);

}