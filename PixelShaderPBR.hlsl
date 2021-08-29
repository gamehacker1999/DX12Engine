#include "Common.hlsl"
#include "Lighting.hlsli"

cbuffer LightingData : register(b1)
{
	float3 cameraPosition;
	uint lightCount;
};

struct Index
{
	uint index;
};

ConstantBuffer <Index>entityIndex: register(b0);

struct IndirectLighting
{
    bool enableIndirectLighting;
    bool inlineRaytrace;
};
ConstantBuffer<IndirectLighting> indLighting : register(b2);

RaytracingAccelerationStructure SceneBVH : register(t0, space4);

struct VertexToPixel
{
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float3 normal:  NORMAL;
	float3 tangent: TANGENT;
	float3 worldPosition: POSITION;
	float2 uv: TEXCOORD;

};

#define TILE_SIZE 8

Texture2D material[]: register(t0);
TextureCube irradianceMap: register(t0, space1);
TextureCube prefilteredMap: register(t1, space1);

Texture2D vmfMap: register(t0, space3);
Texture2D prefilteredRoughnessMap: register(t1, space3);

StructuredBuffer<uint> LightIndices : register(t1, space2);

float4 main(VertexToPixel input) : SV_TARGET
{
    uint2 location = uint2(input.position.xy);
    uint2 tileID = location / uint2(TILE_SIZE, TILE_SIZE);
    uint numberOfTilesX = WIDTH / TILE_SIZE;
    uint tileIndex = tileID.y * numberOfTilesX + tileID.x;

	uint index = entityIndex.index;

	float4 surfaceColor = material[index+0].SampleBias(basicSampler,input.uv, -1.0f);

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
    N = finalNormal;
    N = normalize(N); //normalizing the normal

	//getting the metalness of the pixel
	float3 metalColor = material[index + 3].Sample(basicSampler, input.uv).xyz;

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, surfaceColor.xyz, metalColor);

	//getting the roughness of pixel
	float roughness = material[index + 2].Sample(basicSampler, input.uv).x;
	
    float3 diffuseColor = surfaceColor.rgb * (1 - metalColor);

	float newRoughness = prefilteredRoughnessMap.Sample(basicSampler, input.uv).x;
	float vmf = vmfMap.Sample(basicSampler, input.uv).x;
	
	//step 1 --- Solving the radiance integral for direct lighting, the integral is just the number of light sources
	// the solid angle on the hemisphere in infinitely small, so the wi is just a direction vector
	//for now radiance is just the color of the direction light, the diffuse part is lambertian*c/pi
	//specular is calculated by cook torrence, which ontains in ks term in is due to fresnel


	float3 V = normalize(cameraPosition - input.worldPosition); //view vector
	float3 R = reflect(-V, N); //reflect R over N
	
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	
	
    float ndotv = saturate(dot(N, V));
	
    float2 ltcUV = float2(roughness, sqrt(1-ndotv));
    ltcUV = ltcUV * LUT_SCALE + LUT_BIAS;
	
	ltcUV.y = 1.0f - ltcUV.y;

    float4 t1 = LtcLUT.Sample(basicSampler, ltcUV);
    float4 t2 = LtcLUT2.Sample(basicSampler, ltcUV);
    float4 envBRDF = brdfLUT.Sample(basicSampler, float2(ndotv, roughness));
	
    float3 shadow = float3(1, 1, 1);
	
	if(false && indLighting.enableIndirectLighting)
    {
    
	    //creating the rayDescription
        RayDesc ray;
        ray.Origin = input.worldPosition + N;
        ray.Direction = normalize(-lights[0].direction);
        ray.TMin = 1;
        ray.TMax = 100000;
	
		// Instantiate ray query object.
		// Template parameter allows driver to generate a specialized
		// implementation.
        RayQuery < RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > q;
	
	    // Set up a trace.  No work is done yet.
        q.TraceRayInline(
        SceneBVH,
        RAY_FLAG_NONE, // OR'd with flags above
        1,
        ray);

		// Proceed() below is where behind-the-scenes traversal happens,
		// including the heaviest of any driver inlined code.
		// In this simplest of scenarios, Proceed() only needs
		// to be called once rather than a loop:
		// Based on the template specialization above,
		// traversal completion is guaranteed.
        q.Proceed();

		// Examine and act on the result of the traversal.
		// Was a hit committed?
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            shadow = float3(0.2, 0.2, 0.2);

        }
    }
	
    uint offset = tileIndex * 1024;
	[loop]
     for (uint i = 0; i < 1024 && LightIndices[offset + i] != -1; i++)
     {
         uint lightIndex = LightIndices[offset + i];
	
         switch (lights[lightIndex].type)
         {
            case LIGHT_TYPE_DIR:
                Lo += DirectLightPBR(lights[lightIndex], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0, TBN);
                break;
            case LIGHT_TYPE_SPOT:
                Lo += SpotLightPBR(lights[lightIndex], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0, TBN);
                break;
            case LIGHT_TYPE_POINT:
                Lo += PointLightPBR(lights[lightIndex], N, input.worldPosition, cameraPosition,
			roughness, metalColor.r, surfaceColor.xyz, f0, TBN);
                break;
            case LIGHT_TYPE_AREA_RECT:
                Lo += RectAreaLightPBR(lights[lightIndex], N, V, input.worldPosition, 
			cameraPosition, roughness, metalColor.x, surfaceColor.rgb, f0, t1, envBRDF, brdfSampler, material[4], TBN);
                break;
            case LIGHT_TYPE_AREA_DISK:
                Lo += DiskAreaLightPBR(lights[lightIndex], N, V, input.worldPosition, 
			cameraPosition, roughness, metalColor.x, surfaceColor.rgb, f0, t1, envBRDF, brdfSampler);
                break;
         }
     }
	
	


	float3 ksIndirect = FresnelRoughness(dot(N, V), f0, roughness);

	float3 kdIndirect = float3(1.0f, 1.0f, 1.0f) - ksIndirect;

	kdIndirect *= (1 - metalColor);

	kdIndirect *= surfaceColor.rgb / PI;

	float3 irradiance = irradianceMap.Sample(basicSampler, N).rgb;

	float3 diffuseIndirect = surfaceColor.rgb * irradiance;

	float3 prefilteredColor = prefilteredMap.SampleLevel(basicSampler, R, roughness * 4.0).rgb;

	float3 specularIndirectSingleScattering = (ksIndirect * envBRDF.x + envBRDF.y);
    float ess = envBRDF.x + envBRDF.y;
    float ems = 1.0f - ess;
	
    float3 Favg = ksIndirect + (1.0f / 21.f) * (1.0f - ksIndirect);
	
    float3 fms = specularIndirectSingleScattering * Favg / (1 - Favg * (1 - ess));
	
    float3 lss = specularIndirectSingleScattering * prefilteredColor;
    float3 lms = fms * ems * irradiance;
	
    float3 specularIndirect = lss + lms;

	float3 ambientIndirect = (diffuseIndirect); 

    float3 color = Lo;
	
	if(indLighting.enableIndirectLighting)
        color+= ambientIndirect;
	
    //color *= shadow;
	color = pow(abs(color), 1.f / 1);

	return float4(color, surfaceColor.w);
}