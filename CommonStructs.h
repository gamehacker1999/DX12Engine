#pragma once
#include<DirectXMath.h>
using namespace DirectX;

#ifdef __cplusplus


#define MATRIX XMFLOAT4X4;
#define FLOAT4 XMFLOAT4;
#define FLOAT3 XMFLOAT3;
#define FLOAT2 XMFLOAT2;

#else

#define MATRIX matrix;
#define FLOAT4 float4;
#define FLOAT3 float3;
#define FLOAT2 float2;
#endif // __cplusplus
