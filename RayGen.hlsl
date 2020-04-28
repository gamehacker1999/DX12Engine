#include "RTUtils.hlsli"

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0);
RWTexture2D< float4 > gDiffuse : register(u1);
RWTexture2D< float4 > gPosition : register(u2);
RWTexture2D< float4 > gNormal : register(u3);
RWTexture2D< float4 > gAlbedo : register(u4);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

struct RayTraceCameraData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
};

cbuffer LightData: register(b1)
{
    DirectionalLight light1;
    float3 cameraPosition;
};



ConstantBuffer<RayTraceCameraData> cameraData: register(b0);

float ShootShadowRays(float3 origin, float3 direction, float minT, float maxT)
{
    //initialize the hit payload
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;

    /**/RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = minT;
    ray.TMax = maxT;

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, ray, shadowPayload);

    return shadowPayload.isHit?0.3:1.0;
}


float3 DiffuseShade(float3 pos, float3 norm, float3 albedo, inout uint seed)
{
    float3 L = -light1.direction;

    float NdotL = saturate(dot(norm, L));

    float factor = ShootShadowRays(pos, L, 1.0e-4f, 10000000);

    //float factor = isHit ? 0.3 : 1.0;
    float3 rayColor = light1.diffuse*factor;

    return (NdotL * rayColor * (albedo));
}

[shader("raygeneration")] 
void RayGen() {
  // Initialize the ray payload
  HitInfo payload;
  payload.color = float3(0, 0, 0);
  payload.rayDepth = 0.0;
  payload.normal = float3(0, 0, 0);


  // Get the location within the dispatched 2D grid of work items
  // (often maps to pixels, so this could represent a pixel coordinate).
  uint2 launchIndex = DispatchRaysIndex().xy;
  float2 dims = float2(DispatchRaysDimensions().xy);
  float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);

  uint rndseed = initRand(launchIndex.x, launchIndex.y);

  float3 pos = gPosition[launchIndex].xyz;
  float3 norm = gNormal[launchIndex].xyz;
  float3 albedo = gAlbedo[launchIndex].xyz;
  float3 diffuse = gDiffuse[launchIndex].xyz;

  float3 color = float3(0, 0, 0);

  if (norm.x == 0 && norm.y == 0 && norm.z == 0)
  {
      color = albedo;
  }

  else
  {
      color = diffuse;// DiffuseShade(pos, norm, albedo, rndseed);

      float3 giVal = float3(0, 0, 0);
      for (int i = 0; i < 32; i++)
      {
          float3 giDir = getCosHemisphereSample(rndseed, norm);

          HitInfo giPayload = { float3(0,0,0),payload.rayDepth,rndseed,pos,norm,albedo };

          /**/RayDesc ray2;
          ray2.Origin = pos;
          ray2.Direction = giDir;
          ray2.TMin = 0.01;
          ray2.TMax = 100000;


          TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray2, giPayload);

          giVal += giPayload.color;
          //color += albedo * giPayload.color;
      }
      giVal = giVal / 32.f;
      color += albedo * giVal;;
  }
  

  //float3 color = DiffuseShade(pos, norm, albedo, rndseed,light1);
  //
  //float3 giDir = getCosHemisphereSample(rndseed, norm);
  //float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
  //
  //HitInfo giPayload = { float3(0,0,0),payload.rayDepth,rndseed,pos,norm,albedo };
  //
  ///**/RayDesc ray2;
  //ray2.Origin = payload.currentPosition;
  //ray2.Direction = giDir;
  //ray2.TMin = 0.01;
  //ray2.TMax = 100000;
  //
  //
  //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray2, giPayload);
  //
  //color += albedo * giPayload.color;




  //creating the rayDescription
 //RayDesc ray;
 //ray.Origin = mul(float4(0, 0, 0, 1), cameraData.iView);
 //payload.currentPosition = ray.Origin;
 //float4 target = mul(float4(d.x, -d.y, 1, 1), cameraData.iProj);
 //ray.Direction = mul(float4(target.xyz, 0), cameraData.iView);
 //ray.TMin = 0.01;
 //ray.TMax = 100000;
 //payload.rndseed = rndseed;
 //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, payload);
 //
 //float3 giDir = getCosHemisphereSample(payload.rndseed, payload.normal);
 ////float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
 //
 //HitInfo giPayload = { float3(0,0,0),payload.rayDepth,payload.rndseed,payload.currentPosition,payload.normal,payload.diffuseColor };
 //
 ///**/RayDesc ray2;
 //ray.Origin = payload.currentPosition;
 //ray.Direction = giDir;
 //ray.TMin = 0.01;
 //ray.TMax = 100000;
 //
 //
 //TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray2, giPayload);

 // if(giPayload.normal.x!=0&&giPayload.normal.y != 0&& giPayload.normal.z != 0)
  //payload.colorAndDistance += float4(payload.diffuseColor, 1.0) * giPayload.colorAndDistance;


  gOutput[launchIndex] = float4(color,1.f);
}
