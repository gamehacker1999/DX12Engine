#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
#include<WICTextureLoader.h>
#include<DDSTextureLoader.h>
#include<fstream>
#include<DirectXHelpers.h>
#include<string>
#include"Mesh.h"

struct VolumeData
{
	XMFLOAT4X4 model;
	XMFLOAT4X4 inverseModel;
	XMFLOAT4X4 view;
	XMFLOAT4X4 viewInv;
	XMFLOAT4X4 proj;
	XMFLOAT3 cameraPosition;
	float focalLength;
	float time;
};
class RaymarchedVolume
{
	//ComPtr<ID3D12Resource> skyboxTexResource;
	ManagedResource volumeTexResource;
	//ManagedResource volumeTexResource;
	std::shared_ptr<Mesh> volumeMesh;
	ComPtr<ID3D12PipelineState> volumeRenderPipelineState;
	ComPtr<ID3D12RootSignature> volumeRenderRootSignature;
	ComPtr<ID3D12Resource> volumeDataResource;
	ComPtr<ID3D12Resource> textureUpload;

	UINT8* volumeBufferBegin;
	DescriptorHeapWrapper descriptorHeap;
	//ManagedResource constantBufferResource;
	VolumeData volumeData;

	XMFLOAT3 position;

	//skybox index in the ring buffer

public:
	RaymarchedVolume(std::wstring volumeTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& volumePSO,
		ComPtr<ID3D12RootSignature> volumeRoot, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue,
		DescriptorHeapWrapper& mainBufferHeap,ComPtr<ID3D12GraphicsCommandList> commandList);

	void SetPosition(XMFLOAT3 pos);
	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12Resource>& GetConstantBuffer();
	DescriptorHeapWrapper& GetDescriptorHeap();

	ManagedResource& GetVolumeTexture();
	std::shared_ptr<Mesh>& GetMesh();

	void PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition,float totalTime);

	UINT volumeTextureIndex;
};

