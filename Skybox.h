#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
#include<WICTextureLoader.h>
#include<DDSTextureLoader.h>
#include<DirectXHelpers.h>
#include<string>
#include"Mesh.h"
#include"Environment.h"
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

	//skybox environment for image based lighting
	bool hasEnvironmentMaps;
	std::unique_ptr<Environment> environment;
	DescriptorHeapWrapper dummyHeap;


	
public:
	Skybox(std::wstring skyboxTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& skyboxPSO,
		ComPtr<ID3D12RootSignature> skyboxRoot, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue,
		DescriptorHeapWrapper& mainBufferHeap, bool isCubeMap = true);

	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12Resource>& GetConstantBuffer();
	DescriptorHeapWrapper& GetDescriptorHeap();

	void CreateEnvironment(ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12Device> device, ComPtr<ID3D12RootSignature> prefilterRootSignature, ComPtr<ID3D12RootSignature> brdfRootSignature,
		ComPtr<ID3D12PipelineState> irradiencePSO, ComPtr<ID3D12PipelineState> prefilteredMapPSO,
		ComPtr<ID3D12PipelineState> brdfLUTPSO, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);

	ManagedResource& GetSkyboxTexture();
	std::shared_ptr<Mesh>& GetMesh();

	void PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition);

	DescriptorHeapWrapper GetEnvironmentHeap();

	UINT skyboxTextureIndex;
	UINT environmentTexturesIndex;
};

