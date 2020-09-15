#include "Lighting.hlsli"

Texture2D depthMap : register(t1);
RWStructuredBuffer<uint> LightIndices : register(u0);
RWTexture2D<uint2> LightGrid : register(u1);

//groupshared variables
groupshared uint minDepth;
groupshared uint maxDepth;
groupshared uint visibleLightCount;
groupshared uint visibleLightIndices[1024];
groupshared matrix viewProjection;
groupshared Frustum frustum;
groupshared ViewFrustum viewFrustum;
groupshared float4 frustumPlanes[6];

// directx ndc, minDepth = 0.0, maxDepth = 1.0
static const float2 ndcUpperLeft = float2(-1.0, 1.0);
static const float nearPlane = 0.0;
static const float farPlane = 1.0;

cbuffer externalData : register(b0)
{
    matrix view;
    matrix projection;
    matrix inverseProjection;
    float3 cameraPosition;
    int lightCount;
};

// Convert clip space coordinates to view space
float4 ClipToView(float4 clip)
{
    // View space position.
    float4 view = mul(clip, inverseProjection);
    // Perspective projection.
    view = view / view.w;
 
    return view;
}
 
// Convert screen space coordinates to view space.
float4 ScreenToView(float4 screen)
{
    // Convert to normalized texture coordinates
    float2 texCoord = screen.xy / float2(1280,720);
 
    // Convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);
 
    return ClipToView(clip);
}

// Construct view frustum
ViewFrustum CreateFrustum(int2 tileID, float fMinDepth, float fMaxDepth)
{

    matrix inverseProjView = InvertMatrix(viewProjection);

    float2 ndcSizePerTile = 2 * float2(TILE_SIZE, TILE_SIZE) / float2(1280,720);

    float2 ndcPoints[4]; // corners of tile in ndc
    ndcPoints[0] = ndcUpperLeft + tileID * ndcSizePerTile; // upper left
    ndcPoints[1] = float2(ndcPoints[0].x + ndcSizePerTile.x, ndcPoints[0].y); // upper right
    ndcPoints[2] = ndcPoints[0] + ndcSizePerTile;
    ndcPoints[3] = float2(ndcPoints[0].x, ndcPoints[0].y + ndcSizePerTile.y); // lower left

    ViewFrustum frustum;

    float4 temp;
    for (int i = 0; i < 4; i++)
    {
        temp = mul(float4(ndcPoints[i], fMinDepth, 1.0), inverseProjView);
        frustum.points[i] = temp.xyz / temp.w;
        temp = mul(float4(ndcPoints[i], fMaxDepth, 1.0), inverseProjView);
        frustum.points[i + 4] = temp.xyz / temp.w;
    }

    float3 temp_normal;
    for (int j = 0; j < 4; j++)
    {
		//Cax+Cby+Ccz+Cd = 0, planes[i] = (Ca, Cb, Cc, Cd)
		// temp_normal: normal without normalization
        temp_normal = -cross(frustum.points[j] - cameraPosition, frustum.points[j + 1] - cameraPosition);
        temp_normal = normalize(temp_normal);
        frustum.planes[j] = float4(temp_normal, -dot(temp_normal, frustum.points[j]));
    }
	// near plane
	{
        temp_normal = -cross(frustum.points[1] - frustum.points[0], frustum.points[3] - frustum.points[0]);
        temp_normal = normalize(temp_normal);
        frustum.planes[4] = float4(temp_normal, -dot(temp_normal, frustum.points[0]));
    }
	// far plane
	{
        temp_normal = -cross(frustum.points[7] - frustum.points[4], frustum.points[5] - frustum.points[4]);
        temp_normal = normalize(temp_normal);
        frustum.planes[5] = float4(temp_normal, -dot(temp_normal, frustum.points[4]));
    }

    return frustum;
}

bool IsCollided(Light light, ViewFrustum frustum)
{
    bool result = true;

    for (int i = 0; i < 6; i++)
    {
        if (dot(light.position, frustum.planes[i].xyz) + frustum.planes[i].w < -light.range)
        {
            result = false;
            break;
        }
    }

    if (!result)
    {
        return false;
    }

    return true;
}


[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, // 3D index of the thread group in the dispatch.
uint3 groupThreadID : SV_GroupThreadID, // 3D index of local thread ID in a thread group.
uint3 dispatchThreadID : SV_DispatchThreadID, // 3D index of global thread ID in the dispatch.
uint groupIndex : SV_GroupIndex)
{
    uint2 location = (dispatchThreadID.xy);
    uint2 itemID = (groupThreadID.xy);
    uint2 tileID = (groupID.xy);
    uint2 tileNumber = (uint2(1280 / TILE_SIZE, 720 / TILE_SIZE));
    uint index = tileID.y * tileNumber.x + tileID.x;
    
    float depth = depthMap.Load(uint3(location, 0)).r;
    uint udepth = asuint(depth);
    
    if (groupIndex == 0)
    {
        minDepth = 0xffffffff;
        maxDepth = 0;
        visibleLightCount = 0;
        viewProjection = mul(view, projection);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    //Calculating the min and max depth values for the tile
    InterlockedMin(minDepth, udepth);
    InterlockedMax(maxDepth, udepth);

    
    GroupMemoryBarrierWithGroupSync();
   
    float fMinDepth = asfloat(minDepth);
    float fMaxDepth = asfloat(maxDepth);
    
    if(fMaxDepth <= fMinDepth)
        fMaxDepth = fMinDepth;
    
        //creating the frustums
    if (groupIndex == 0)
    {
      // View space eye position is always at the origin.
       const float3 eyePos = float3(0, 0, 0);
     
       float4 screenSpace[4];
        screenSpace[0] = float4(groupID.xy * TILE_SIZE, 1.0f, 1.0f);
        screenSpace[1] = float4(float2(groupID.x + 1, groupID.y) * TILE_SIZE, 1.0f, 1.0f);
        screenSpace[2] = float4(float2(groupID.x, groupID.y + 1) * TILE_SIZE, 1.0f, 1.0f);
        screenSpace[3] = float4(float2(groupID.x + 1, groupID.y + 1) * TILE_SIZE, 1.0f, 1.0f);
      
      float3 viewSpace[4];
      for (int i = 0; i < 4; i++)
      {
          viewSpace[i] = ScreenToView(screenSpace[i]).xyz;
      }
      
      
      frustum.frustumPlanes[0] = ComputePlane(eyePos, viewSpace[2], viewSpace[0]);
      frustum.frustumPlanes[1] = ComputePlane(eyePos, viewSpace[1], viewSpace[3]);
      frustum.frustumPlanes[2] = ComputePlane(eyePos, viewSpace[0], viewSpace[1]);
      frustum.frustumPlanes[3] = ComputePlane(eyePos, viewSpace[3], viewSpace[2]);
        
        //viewFrustum = CreateFrustum(tileID, fMinDepth, fMaxDepth);


    }
    
    GroupMemoryBarrierWithGroupSync();
   
    // Convert depth values to view space.
    float minDepthVS = ScreenToView(float4(0, 0, fMinDepth, 1)).z;
    float maxDepthVS = ScreenToView(float4(0, 0, fMaxDepth, 1)).z;
    float nearClipVS = ScreenToView(float4(0, 0, 0, 1)).z;
    
    // Clipping plane for minimum depth value 
    Plane minPlane = { float3(0, 0, 1), fMinDepth };
    
    
    uint threadCount = TILE_SIZE * TILE_SIZE;
    uint passCount = (lightCount + threadCount - 1) / threadCount;
    for (uint i = 0; i < passCount; i ++)
    {
        uint lightIndex = i * threadCount + groupIndex;
        if (lightIndex >= lightCount)
        {
            break;
        }
        
        float4 vsPos = mul(float4(lights[lightIndex].position, 1.0), view);
        
        bool interSects = false;
        
        switch (lights[lightIndex].type)
        {
            case LIGHT_TYPE_DIR:
            {
               uint offset;
               InterlockedAdd(visibleLightCount, 1, offset);
               visibleLightIndices[offset] = lightIndex;
            }
            break;
    
            case LIGHT_TYPE_SPOT:
            {
                     
               interSects = false;
            }
            break;
            
            case LIGHT_TYPE_POINT:
            {      
               Sphere sphere;
               sphere.c = vsPos.xyz;
               sphere.r = lights[lightIndex].range;
               if (SphereInsideFrustum(sphere, frustum, nearClipVS, maxDepthVS) && !SphereInsidePlane(sphere, minPlane))
               {
                  uint offset;
                  InterlockedAdd(visibleLightCount, 1, offset);
                  visibleLightIndices[offset] = lightIndex;
               }
             }
             break;
            
            case LIGHT_TYPE_AREA_RECT:    
            {
                 // Add all rect lights for now    
                uint offset;
                InterlockedAdd(visibleLightCount, 1, offset);
                visibleLightIndices[offset] = lightIndex;
             }
            break;
                     
            case LIGHT_TYPE_AREA_DISK:
            {
                                // Add all rect lights for now    
                   uint offset;
                   InterlockedAdd(visibleLightCount, 1, offset);
                   visibleLightIndices[offset] = lightIndex;
            }
            break;
           
        }
        
        if(interSects)
        {
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if(groupIndex == 0)
    {
        uint offset = 1024 * index;
        for (uint i = 0; i < visibleLightCount;i++)
        {
            LightIndices[offset + i] = visibleLightIndices[i];
        }
        
        if (visibleLightCount != 1024)
        {
            LightIndices[offset + visibleLightCount] = -1;
        }
    
    }
    
   //uint listOffset = 1024 * index;
   //
   //for (uint i = groupIndex; i < lightCount && visibleLightCount < 1024; i += TILE_SIZE)
   //{
   //    switch (lights[i].type)
   //    {
   //        case LIGHT_TYPE_DIR:
   //        {
   //            uint offset;
   //            InterlockedAdd(visibleLightCount, 1, offset);
   //            LightIndices[listOffset + offset] = i;
   //        }
   //        break;
   //
   //        case LIGHT_TYPE_SPOT:
   //        {
   //         
   //        }
   //        break;
   //
   //        case LIGHT_TYPE_POINT:
   //        {      
   //
   //           if (IsCollided(lights[i],viewFrustum))
   //           {
   //               uint offset;
   //               InterlockedAdd(visibleLightCount, 1, offset);
   //                   LightIndices[listOffset + offset] = i;
   //           }
   //       }
   //       break;
   //
   //       case LIGHT_TYPE_AREA_RECT:
   //        {
   //            //Add all rect lights for now    
   //           uint offset;
   //           InterlockedAdd(visibleLightCount, 1, offset);
   //           LightIndices[listOffset + offset] = i;
   //        }
   //        break;
   //        
   //       case LIGHT_TYPE_AREA_DISK:
   //       {
   //       }
   //       break;
   //
   //    }
   //}
   //
   //GroupMemoryBarrierWithGroupSync();
   //
   //if(groupIndex == 0)
   //{   
   //    if (visibleLightCount != 1024)
   //    {
   //        LightIndices[listOffset + visibleLightCount] = -1;
   //    }
   //
   //}

}