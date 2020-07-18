#ifdef __cplusplus
#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
using namespace DirectX;

static const float PI = 3.14159265f;

#define MAX_LIGHTS 128

#define LIGHT_TYPE_DIR 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_AREA_RECT 3


struct RectLight
{
	float width;
	float height;
	float rotY;
	float rotZ;
	XMFLOAT3 position;
	float padding;
};

struct Light
{
	int type;
	XMFLOAT3 direction;
	float range;
	XMFLOAT3 position;
	float intensity;
	XMFLOAT3 diffuse;
	float spotFalloff;
	XMFLOAT3 color;

	RectLight rectLight;
};

struct DirectionalLight
{
	XMFLOAT4 ambientColor;
	XMFLOAT4 diffuse;
	XMFLOAT4 specularity;
	XMFLOAT3 direction;
	float padding;
};

#else
struct Rect
{
	float halfx;
	float halfy;
	float3 dirx;
	float3 diry;
	float3 center;
};

struct RectLight
{
	float width;
	float height;
	float rotY;
	float rotZ;
	float3 position;
	float padding;
};

struct Light
{
	int type;
	float3 direction;
	float range;
	float3 position;
	float intensity;
	float3 diffuse;
	float spotFalloff;
	float3 color;

	RectLight rectLight;
};
#endif
