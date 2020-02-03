#pragma once
#include"DX12Helper.h"
#include<string>
#include"DescriptorHeapWrapper.h"

using namespace DirectX;
class Material
{
	//ComPtr<ID3D12Resource> diffuseTexture;
	ManagedResource diffuseTexture;
	ManagedResource normalTexture;
	ManagedResource roughnessTexture;
	ManagedResource metallnessTexture;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	DescriptorHeapWrapper descriptorHeap;
	UINT materialOffset;
	UINT materialIndex;
	UINT diffuseTextureIndex;
	UINT numTextures;
	D3D12_SHADER_RESOURCE_VIEW_DESC diffuseSRV;

public:
	Material(ComPtr<ID3D12Device>& device,ComPtr<ID3D12CommandQueue>& commandQueue,DescriptorHeapWrapper& mainBufferHeap, 
		ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig,
		std::wstring diffuse, std::wstring normal = L"default", std::wstring roughness = L"default", 
		std::wstring metallnes = L"default");

	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	DescriptorHeapWrapper& GetDescriptorHeap();
	UINT GetMaterialOffset();
	UINT GetDiffuseTextureOffset();
};

