#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
using namespace DirectX;

//struct to define a particle
struct Particle
{
	float spawnTime;
	XMFLOAT3 startPosition;

	float rotationStart;
	XMFLOAT3 startVelocity;

	float rotationEnd;
	XMFLOAT3 padding;
};