#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
#include"Mesh.h"
#include<memory>

struct EnvironmentData
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT3 cameraPos;
	float padding[13];
};

class Environment
{
	std::vector<XMFLOAT4X4> cubemapViews;
	XMFLOAT4X4 cubemapProj;
	ManagedResource irradienceMapTexture;
	ManagedResource irradienceDepthStencil;
	//ManagedResource irradienceSRV;
	//ManagedResource irradienceRTV[6];
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	

	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;

	//prefiltered environment map textures
	ManagedResource prefilteredMapTextures;
	//ManagedResource prefilteredMapSRV;
	//ManagedResource prefilteredMapRTV[6];

	//brdf LUT
	ManagedResource environmentBRDFLUT;
	//ManagedResource environmentBRDFSRV;
	//ManagedResource environmentBRDFRTV;

	//pipeline state objects
	ComPtr<ID3D12PipelineState> irradiencePSO;
	ComPtr<ID3D12PipelineState> prefilteredMapPSO;
	ComPtr<ID3D12PipelineState> brdfLUTPSO;

	//root signatures
	ComPtr<ID3D12RootSignature> irradianceRootSignature;
	ComPtr<ID3D12RootSignature> prefilteredRootSignature;
	ComPtr<ID3D12RootSignature> brdfRootSignature;

	//descriptor heaps
	DescriptorHeapWrapper srvDescriptorHeap;
	DescriptorHeapWrapper dsvDescriptorHeap;
	DescriptorHeapWrapper rtvDescriptorHeap;

	//shaderdata
	EnvironmentData environmentData;
	UINT8* environmentDataBegin;
	ComPtr<ID3D12Resource> constantBufferResource;

	//skyboxdata
	D3D12_VERTEX_BUFFER_VIEW skyboxCube;
	D3D12_INDEX_BUFFER_VIEW skyboxIndexBuffer;
	UINT indexCount;

	std::shared_ptr<Mesh> cube;

public:
	Environment(ComPtr<ID3D12RootSignature> irradianceRootSignature, ComPtr<ID3D12RootSignature> prefilteredRootSignature, ComPtr<ID3D12RootSignature> brdfRootSignature,
		ComPtr<ID3D12PipelineState> irradiencePSO, ComPtr<ID3D12PipelineState> prefilteredMapPSO,
		ComPtr<ID3D12PipelineState> brdfLUTPSO,ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> commandList,
		CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle,
		D3D12_VERTEX_BUFFER_VIEW skyboxCube,D3D12_INDEX_BUFFER_VIEW indexBuffer,UINT indexCount);
	~Environment();
	void CreateIrradianceMap(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	void CreatePrefilteredEnvironmentMap(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle,D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	void CreateBRDFLut(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	DescriptorHeapWrapper GetSRVDescriptorHeap();


};

