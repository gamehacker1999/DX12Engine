#pragma once
#include<DirectXMath.h>
using namespace DirectX;


#ifdef __cplusplus

typedef XMFLOAT4X4 MATRIX ;
typedef XMFLOAT4 FLOAT4;
typedef XMFLOAT3 FLOAT3 ;
typedef XMFLOAT2 FLOAT2 ;

#else

typedef matrix MATRIX;
typedef float4 FLOAT4;
typedef float3 FLOAT3;
typedef float2 FLOAT2;
#endif // __cplusplus

