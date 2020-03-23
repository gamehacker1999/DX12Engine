#pragma once

#include<DirectXMath.h>

//creating a flocker component to describe a flocker
struct Flocker 
{
	DirectX::XMFLOAT3 pos;
	float mass;
	DirectX::XMFLOAT3 vel;
	float maxSpeed;
	DirectX::XMFLOAT3 acceleration;
	float safeDistance;
};