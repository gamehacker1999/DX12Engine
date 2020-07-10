
#include "HitGroupIncludes.hlsli"

cbuffer LightData: register(b0)
{
    DirectionalLight light1;
    float3 cameraPosition;
};

[shader("closesthit")]
void GBufferClosestHit(inout GbufferPayload payload, Attributes attrib)
{   
	float3 position;
	float3 surfaceColor;
	float3 normal;
	float3 metalColor;
	float roughness;
    
	GetPrimitiveProperties(surfaceColor, position, normal, metalColor, roughness, attrib);

    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.roughnessMetallic = float4(metalColor, roughness);
    payload.albedo = surfaceColor;
    payload.position = worldOrigin;
    payload.normal = normal;
}



[shader("closesthit")]
void GBufferPlaneClosestHit(inout GbufferPayload payload, Attributes attrib)
{

  //  /**/float3 barycentrics =
  //      float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
  //
  //  const float3 A = float3(1, 0, 0);
  //  const float3 B = float3(0, 1, 0);
  //  const float3 C = float3(0, 0, 1);
  //
  //  //hardcoding the light position for now
  //  float3 lightPos = float3(-20, 20, 0);
  //  float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
  //  float3 lightDirection = normalize(lightPos - worldOrigin);
  //
  //  uint vertID = PrimitiveIndex() * 3;
  //  float3 normal = vertex[vertID].Normal * barycentrics.x + vertex[vertID + 1].Normal * barycentrics.y + vertex[vertID + 2].Normal * barycentrics.z;
  //  normal = ConvertFromObjectToWorld(normal);
  //  float3 position = vertex[vertID].Position * barycentrics.x + vertex[vertID + 1].Position * barycentrics.y + vertex[vertID + 2].Position * barycentrics.z;
  //  position = ConvertFromObjectToWorld(position);
  //  float3 tangent = vertex[vertID].Tangent * barycentrics.x + vertex[vertID + 1].Tangent * barycentrics.y + vertex[vertID + 2].Tangent * barycentrics.z;
  //  tangent = ConvertFromObjectToWorld(tangent);
  //  float2 uv = vertex[vertID].UV * barycentrics.x + vertex[vertID + 1].UV * barycentrics.y + vertex[vertID + 2].UV * barycentrics.z;
  //
  //  uint index = entityIndex.index;
  //  float3 texColor = material[index + 0].SampleLevel(basicSampler, uv, 0).rgb;
  //  float3 normalColor = material[index + 1].SampleLevel(basicSampler, uv, 0).xyz;
  //  float3 unpackedNormal = normalColor * 2.0f - 1.0f;
  //
  //  //orthonormalizing T, B and N using the gram-schimdt process
  //  float3 N = normalize(normal);
  //  float3 T = tangent - dot(tangent, N) * N;
  //  T = normalize(T);
  //  float3 Bi = normalize(cross(T, N));
  //
  //  float3x3 TBN = float3x3(T, Bi, N); //getting the tbn matrix
  //
  //  //transforming normal from map to world space
  //  float3 finalNormal = normalize(mul(unpackedNormal, TBN));
  //
  //  //getting the metalness of the pixel
  //  float3 metalColor = material[index + 3].SampleLevel(basicSampler, uv, 0).xyz;
  //
  //  float3 f0 = float3(0.04f, 0.04f, 0.04f);
  //  f0 = lerp(f0, texColor.xyz, metalColor);
  //
  //  //getting the roughness of pixel
  //  float roughness = material[index + 2].SampleLevel(basicSampler, uv, 0).x;
  //
  //  //hardcoding the light position for now
  //  //float3 lightPos = float3(-20, 20, 0);
  //  //float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
  //  //float3 lightDirection = normalize(lightPos - worldOrigin);
  //  //float3 L = -light1.direction;
  //  //float NdotL = saturate(dot(finalNormal, L));
  //
  //  payload.roughnessMetallic = float4(metalColor, roughness);
  //  payload.albedo = texColor;
  //  payload.position = worldOrigin;
  //  payload.normal = normalize(finalNormal);
  //
  //  float3 L = -light1.direction;
  //
  //  float NdotL = saturate(dot(normal, L));
  //
  //  payload.normal = normal;
  //
  //  float3 rayDir = normalize(WorldRayDirection());
  //
  //  HitInfo reflectedColor;
  //  payload.albedo = texColor;
  //  payload.position = worldOrigin;
  //  payload.normal = normalize(finalNormal);
  //
  //
  // /**/RayDesc ray;
  // ray.Origin = worldOrigin;
  // ray.Direction = L;
  // ray.TMin = 0.01;
  // ray.TMax = 100000;
  //
  // ShadowHitInfo shadowPayload;
  // shadowPayload.isHit = false;
  //
  // TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);
  //
  // float factor = shadowPayload.isHit ? 0.3f : 1.0f;
  // //payload.diffuse = (float3((light1.diffuse * NdotL * factor).rgb) * texColor).rgb / M_PI;
	float3 position;
	float3 surfaceColor;
	float3 normal;
	float3 metalColor;
	float roughness;
    
	GetPrimitiveProperties(surfaceColor, position, normal, metalColor, roughness, attrib);

	float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	payload.roughnessMetallic = float4(metalColor, roughness);
	payload.albedo = surfaceColor;
	payload.position = worldOrigin;
	payload.normal = normal;

}
