#pragma once
#include"DX12Helper.h"
#include<WICTextureLoader.h>
#include<DDSTextureLoader.h>
#include<DirectXHelpers.h>
#include<string>
#include"Mesh.h"
using namespace DirectX;

struct SkyboxData
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT3 cameraPos;
};

class Skybox
{
	ComPtr<ID3D12Resource> skyboxTexResource;
	std::shared_ptr<Mesh> skyboxMesh;
	ComPtr<ID3D12PipelineState> skyBoxPSO;
	ComPtr<ID3D12RootSignature> skyboxRootSignature;
	UINT8* constantBufferBegin;
	ComPtr<ID3D12Resource> constantBufferResource;
	SkyboxData skyboxData;

public:
	Skybox(std::wstring skyboxTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& skyboxPSO, 
		ComPtr<ID3D12RootSignature> skyboxRoot,ComPtr<ID3D12Device>& device,ComPtr<ID3D12CommandQueue>& commandQueue,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& cbvSRVDescriptorHandle);

	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12Resource>& GetConstantBuffer();
	std::shared_ptr<Mesh>& GetMesh();

	void PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition);
};

