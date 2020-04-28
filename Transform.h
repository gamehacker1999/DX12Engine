#pragma once
#include<DirectXMath.h>

//struct to represent the transform component
struct Transform
{
	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 scale;
};