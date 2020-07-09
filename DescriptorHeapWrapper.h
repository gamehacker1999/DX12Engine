#pragma once
#include"DX12Helper.h"
#include<DirectXHelpers.h>

using namespace DirectX;
class DescriptorHeapWrapper
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	D3D12_DESCRIPTOR_HEAP_TYPE heapType;
	UINT numDescriptor;
	bool isShaderVisible;
	UINT handleIncrementSize;
	UINT lastResourceIndex;

public:

	DescriptorHeapWrapper();
	HRESULT Create(ComPtr<ID3D12Device> device, UINT numDesc, bool isShaderVis, D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType();
	ComPtr<ID3D12DescriptorHeap>& GetHeap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index);
	UINT GetLastResourceIndex();
	void IncrementLastResourceIndex(UINT valueToIncrementBy);
	UINT GetDescriptorIncrementSize();
	void CreateDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType, 
		ComPtr<ID3D12Device> device,size_t cbufferSize=0,UINT width = 0, UINT height = 0,UINT firstArraySlice = -1,UINT mipLevel = 0);
	void CreateDescriptor(std::wstring resName, ManagedResource& resource,
		RESOURCE_TYPE resourceType, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue,
		TEXTURE_TYPES type, bool isCube = false);
	void CreateStructuredBuffer(ManagedResource& resource, ComPtr<ID3D12Device> device, UINT numElements, UINT stride, UINT bufferSize);
	void CreateRaytracingAccelerationStructureDescriptor(ComPtr<ID3D12Device5> device, ManagedResource& resource,AccelerationStructureBuffers topLevelASBuffer);
};

