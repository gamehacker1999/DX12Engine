#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
using namespace DirectX;

//struct to define a particle
struct Particle
{
	float spawnTime;
	Vector3 startPosition;

	float rotationStart;
	Vector3 startVelocity;

	float rotationEnd;
	Vector3 padding;
};