#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
using namespace DirectX;

struct DirectionalLight
{
	XMFLOAT4 ambientColor;
	XMFLOAT4 diffuse;
	XMFLOAT4 specularity;
	XMFLOAT3 direction;
};