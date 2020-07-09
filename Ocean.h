#pragma once

#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
#include"Mesh.h"
#include"Camera.h"
#include"Mesh.h"
#include<memory>

//class to generate ocean using either gerstner or IFFT waves
class Ocean
{
	DescriptorHeapWrapper rtvDescriptorHeap;
	DescriptorHeapWrapper srvcbvuavDescriptorHeap;

	//required textures for the fft simulation
	ManagedResource h0Texture;
	ManagedResource h0MinusTexture;

	ManagedResource htxTexture;
	ManagedResource htyTexture;
	ManagedResource htzTexture;

	ManagedResource twiddleTexture;

	ManagedResource pingpong0Texture;

	ManagedResource dyTexture;
	ManagedResource dxTexture;
	ManagedResource dzTexture;

	ManagedResource normalMapTexture;

	ManagedResource foldingMapTexture;

	ComPtr<ID3D12PipelineState> computePipelineState;
	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	ComPtr<ID3D12RootSignature> graphicsRootSignature;
	ComPtr<ID3D12RootSignature> computeRootSignature;

	std::shared_ptr<Mesh> waterMesh;
	XMFLOAT4X4 worldMatrix;
	int texSize;

public:
public:

	//TODO: Add noise textures
	Ocean(std::shared_ptr<Mesh> waterMesh,
		ComPtr<ID3D12RootSignature> graphicsRootSignature, ComPtr<ID3D12PipelineState> graphicsPipelineState,
		ComPtr<ID3D12PipelineState> computePipelineState, ComPtr<ID3D12RootSignature> computeRootSignature,
		ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> graphicsCommandList,ComPtr<ID3D12CommandList> computeCommandList);
	~Ocean();

	void Update(float deltaTime);

	void Draw(std::shared_ptr<Camera> camera, ComPtr<ID3D12GraphicsCommandList> commandList, float deltaTime, float totalTime);

	void CreateH0Texture();
	void CreateHtTexture(float totalTime);
	int CreateBitReversedIndices(int num, int d);
	void CreateTwiddleIndices();
	void RenderFFT(float totalTime);

};

