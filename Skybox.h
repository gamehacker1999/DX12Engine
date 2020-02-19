#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
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
	//ComPtr<ID3D12Resource> skyboxTexResource;
	ManagedResource skyboxTexResource;
	std::shared_ptr<Mesh> skyboxMesh;
	ComPtr<ID3D12PipelineState> skyBoxPSO;
	ComPtr<ID3D12RootSignature> skyboxRootSignature;
	UINT8* constantBufferBegin;
	ComPtr<ID3D12Resource> constantBufferResource;
	DescriptorHeapWrapper descriptorHeap;
	//ManagedResource constantBufferResource;
	SkyboxData skyboxData;

	//skybox index in the ring buffer
	
public:
	Skybox(std::wstring skyboxTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& skyboxPSO,
		ComPtr<ID3D12RootSignature> skyboxRoot, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue,
		DescriptorHeapWrapper& mainBufferHeap);

	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12Resource>& GetConstantBuffer();
	DescriptorHeapWrapper& GetDescriptorHeap();

	ManagedResource& GetSkyboxTexture();
	std::shared_ptr<Mesh>& GetMesh();

	void PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition);

	UINT skyboxTextureIndex;
};

