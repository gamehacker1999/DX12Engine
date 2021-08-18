#include "Common.hlsl"
#include "RTUtils.hlsli"

struct RayTraceExternData
{
    matrix view;
    matrix proj;
    matrix iView;
    matrix iProj;
    matrix prevView;
    matrix prevProj;
    float frameCount;
    int doRestir;
    int outPutColor;
    int doRestirGI;
};


// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gIndirectDiffuseOutput : register(u1);
RWTexture2D<float4> gIndirectSpecularOutput : register(u2);
RWTexture2D<float4> gRoughnessMetallic : register(u3);
RWTexture2D<float4> gPosition : register(u4);
RWTexture2D<float4> gNormal : register(u5);
RWTexture2D<float4> gAlbedo : register(u6);
RWTexture2D<float4> motionBuffer : register(u0, space2);

