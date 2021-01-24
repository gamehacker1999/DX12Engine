#include "Common.hlsl"

#include "HitGroupIncludes.hlsli"


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
