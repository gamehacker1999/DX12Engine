#define TILE_SIZE 8
#define MAX_LIGHTS 20000

#ifdef __cplusplus
#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
using namespace DirectX;

static const float PI = 3.14159265f;

#define LIGHT_TYPE_DIR 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_AREA_RECT 3
#define LIGHT_TYPE_AREA_DISK 4


struct AreaLight
{
	float width;
	float height;
	float rotY;
	float rotZ;
	float rotX;
	Vector3 padding;
};

struct Light
{
	int type;
	Vector3 direction;
	float range;
	Vector3 position;
	float intensity;
	Vector3 diffuse;
	float spotFalloff;
	Vector3 color;

	AreaLight rectLight;
};

struct Decal
{
	Matrix decalProjection;
	Vector3 scale;
};

struct DirectionalLight
{
	Vector4 ambientColor;
	Vector4 diffuse;
	Vector4 specularity;
	Vector3 direction;
	float padding;
};

#else
struct Area
{
	float halfx;
	float halfy;
	float3 dirx;
	float3 diry;
	float3 center;
};

struct AreaLight
{
	float width;
	float height;
	float rotY;
	float rotZ;
	float rotX;
	float3 padding;
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

	AreaLight rectLight;
};

struct Decal
{
	matrix decalProjectionMatrix;
};
#endif
