#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
using namespace DirectX;

#define MAX_LIGHTS 128

#define LIGHT_TYPE_DIR 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_POINT 2

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
};

struct DirectionalLight
{
	XMFLOAT4 ambientColor;
	XMFLOAT4 diffuse;
	XMFLOAT4 specularity;
	XMFLOAT3 direction;
	float padding;
};