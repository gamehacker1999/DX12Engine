
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 worldPos		: TEXCOORD;
};

//variables for the textures
TextureCube skyboxTexture: register(t0);
SamplerState basicSampler: register(s0);

float AngleBetween(in float3 dir0, in float3 dir1)
{
    return acos(dot(dir0, dir1));
}

//-------------------------------------------------------------------------------------------------
// Uses the CIE Clear Sky model to compute a color for a pixel, given a direction + sun direction
//-------------------------------------------------------------------------------------------------
float3 CIEClearSky(in float3 dir, in float3 sunDir)
{
    float cosThetaV = dir.z;

    if (cosThetaV < 0.0f)
        cosThetaV = 0.0f;

    float cosThetaS = sunDir.z;
    float thetaS = acos(cosThetaS);

    float cosGamma = dot(sunDir, dir);
    float gamma = acos(cosGamma);

    float top1 = 0.91f + 10 * exp(-3 * gamma) + 0.45f * (cosGamma * cosGamma);
    float bot1 = 0.91f + 10 * exp(-3 * thetaS) + 0.45f * (cosThetaS * cosThetaS);

    float top2 = 1 - exp(-0.32f / (cosThetaV + 1e-6f));
    float bot2 = 1 - exp(-0.32f);

    return float3(1,1,1) * (top1 * top2) / (bot1 * bot2);

    // Draw a circle for the sun
    
    //if (true)
    //{
    //    float sunGamma = AngleBetween(dir, sunDir);
    //    color = lerp(SunColor, SkyColor, saturate(abs(sunGamma) / SunWidth));
    //}

    //return max(color * lum, 0);
}

float4 main(VertexToPixel input) : SV_TARGET
{
	//sample the skybox color
	float3 skyboxColor = skyboxTexture.Sample(basicSampler,input.worldPos).rgb;
	
	//return the color
	return float4(skyboxColor, 1.0f);

    float3 dir = normalize(input.worldPos);
    
    return float4(CIEClearSky(dir, float3(0,1,1)), 1.0f);
}