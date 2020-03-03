#pragma once

#include<DirectXMath.h>

//creating a flocker component to describe a flocker
struct Flocker 
{
	DirectX::XMFLOAT3 pos;
	int mass;
	DirectX::XMFLOAT3 vel;
	int maxSpeed;
	DirectX::XMFLOAT3 acceleration;
	int safeDistance;
};