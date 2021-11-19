#include "Common.hlsl"

#include "HitGroupIncludes.hlsli"

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    payload.rayDepth++;

    float3 position;
    float4 surfaceColor;
    float3 normal;
    float3 metalColor;
    float roughness;
    
    GetPrimitiveProperties(surfaceColor, position, normal, metalColor, roughness, attrib);
    float3 V = normalize(position - cameraPosition);
    float3 f0 = float3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, surfaceColor, metalColor);
     // Do indirect lighting for global illumination
    float3 color = float3(0, 0, 0);
    
    color += DirectLighting(payload.rndseed, position, normal, V, metalColor.r, surfaceColor, f0, roughness);
    
    //if(payload.rayDepth < 2)
    //{
    //    color += IndirectLighting(payload.rndseed, position, normal, V, metalColor.r,
	//			         surfaceColor, f0, roughness, payload.rayDepth);
    //}

    payload.color = color;

    payload.currentPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(normal);
    payload.diffuseColor = surfaceColor;

}



[shader("closesthit")]
void PlaneClosestHit(inout HitInfo payload, Attributes attrib)
{

}
