#pragma once

#include <DirectXMath.h>

// --------------------------------------------------------
// A custom vertex definition
// --------------------------------------------------------
struct Vertex
{
	DirectX::XMFLOAT3 Position;	    // The position of the vertex
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 UV;
};