#pragma once

#include <DirectXMath.h>
#include "SimpleMath.h"
// --------------------------------------------------------
// A custom vertex definition
// --------------------------------------------------------
struct Vertex
{
	DirectX::SimpleMath::Vector3 Position;	    // The position of the vertex
	DirectX::SimpleMath::Vector3 Normal;
	DirectX::SimpleMath::Vector3 Tangent;
	DirectX::SimpleMath::Vector2 UV;
};