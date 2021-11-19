#include"Common.hlsl"

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
    //if (InstanceID() == hit.primitiveIndex)
      //  hit.isHit = false;
    
    //else
        hit.isHit = true;
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit : SV_RayPayload)
{
    hit.isHit = false;
}