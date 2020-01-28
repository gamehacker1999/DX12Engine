#pragma once
#include"DX12Helper.h"
#include<string>

using namespace DirectX;
class Material
{
	ComPtr<ID3D12Resource> diffuseTexture;
	D3D12_SHADER_RESOURCE_VIEW_DESC diffuseSRV;

public:
	Material(std::wstring diffuse, ComPtr<ID3D12Device>& device,ComPtr<ID3D12CommandQueue>& commandQueue,CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle);
};

