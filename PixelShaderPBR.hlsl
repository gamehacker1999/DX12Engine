#include "Lighting.hlsli"

//cbuffer LightData: register(b1)
//{
//	DirectionalLight light1;
//	float3 cameraPosition;
//};

cbuffer LightingData : register(b1)
{
	Light lights[MAX_LIGHTS];
	float3 cameraPosition;
	uint lightCount;
};

struct Index
{
	uint index;
};

ConstantBuffer <Index>entityIndex: register(b0);

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};

Texture2D material[]: register(t0);
SamplerState basicSampler: register(s0);
SamplerState brdfSampler : register(s1);
TextureCube irradianceMap: register(t0, space1);
TextureCube prefilteredMap: register(t1, space1);
Texture2D brdfLUT: register(t2, space1);
Texture2D LtcLUT : register(t3, space1);
Texture2D LtcLUT2 : register(t4, space1);
Texture2D LTCTexture : register(t5, space1);


float4 main(VertexToPixel input) : SV_TARGET
{

	uint index = entityIndex.index;

	float4 surfaceColor = material[index+0].Sample(basicSampler,input.uv);

	surfaceColor = pow(abs(surfaceColor), 2.2);

	//getting the normal texture
	float3 normalColor = material[index + 1].Sample(basicSampler, input.uv).xyz;
	float3 unpackedNormal = normalColor * 2.0f - 1.0f;

	//orthonormalizing T, B and N using the gram-schimdt process
	float3 N = normalize(input.normal);
	float3 T = input.tangent - dot(input.tangent, N) * N;
	T = normalize(T);
	float3 B = normalize(cross(T,N));

	float3x3 TBN = float3x3(T, B, N); //getting the tbn matrix

	//transforming normal from map to world space
	float3 finalNormal = mul(unpackedNormal, TBN);

	//getting the metalness of the pixel
	float3 metalColor = material[index + 3].Sample(basicSampler, input.uv).xyz;

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, surfaceColor.xyz, metalColor);

	//getting the roughness of pixel
	float roughness = material[index + 2].Sample(basicSampler, input.uv).x;
    float3 diffuseColor = surfaceColor.rgb * (1 - metalColor);

	//step 1 --- Solving the radiance integral for direct lighting, the integral is just the number of light sources
	// the solid angle on the hemisphere in infinitely small, so the wi is just a direction vector
	//for now radiance is just the color of the direction light, the diffuse part is lambertian*c/pi
	//specular is calculated by cook torrence, which ontains in ks term in is due to fresnel

	N = finalNormal;
	N = normalize(N); //normalizing the normal
	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float3 R = reflect(-V, N); //reflect R over N
	
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	
	
    float ndotv = saturate(dot(N, V));
	
    float2 ltcUV = ltcCoords(ndotv, roughness);

    //loat2 uv = float2(roughness, sqrt(1-ndotv));
    //v = uv * LUT_SCALE + LUT_BIAS;

    float4 t1 = LtcLUT.Sample(brdfSampler, ltcUV);
    float4 t2 = LtcLUT2.Sample(brdfSampler, ltcUV);
    float2 envBRDF = brdfLUT.Sample(brdfSampler, float2(saturate(dot(N, V)), roughness)).rg;

	for (int i = 0; i < lightCount; i++)
	{
		switch (lights[i].type)
		{
			case LIGHT_TYPE_DIR:
				Lo += DirectLightPBR(lights[i], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0);
				break;
			case LIGHT_TYPE_SPOT:
				Lo += SpotLightPBR(lights[i], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0);
				break;
			case LIGHT_TYPE_POINT:
				Lo += PointLightPBR(lights[i], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0);
				break;
			case LIGHT_TYPE_AREA_RECT:
                Lo += RectAreaLightPBR(lights[i], N, V, input.worldPosition, cameraPosition, roughness, metalColor.x, surfaceColor.rgb, f0, t1, t2, LTCTexture, brdfSampler);
                break;
        }
	}

	float3 ksIndirect = FresnelRoughness(dot(N, V), f0, roughness);

	float3 kdIndirect = float3(1.0f, 1.0f, 1.0f) - ksIndirect;

	kdIndirect *= (1 - metalColor);

	kdIndirect *= surfaceColor.rgb / PI;

	float3 irradiance = irradianceMap.Sample(basicSampler, N).rgb;

	float3 diffuseIndirect = surfaceColor.rgb * irradiance/ 3.14169;

	float3 prefilteredColor = prefilteredMap.SampleLevel(basicSampler, R, roughness * 4.0).rgb;

	float3 specularIndirect = prefilteredColor * (ksIndirect * envBRDF.x + envBRDF.y);

	float3 ambientIndirect = (kdIndirect * diffuseIndirect + specularIndirect); 

    float3 color = Lo;
	color += ambientIndirect;

	color = pow(abs(color), 1.f / 2.2f);

	return float4(color, surfaceColor.w);
}