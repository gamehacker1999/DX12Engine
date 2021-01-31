#pragma once
#include"DX12Helper.h"
#include<DirectXHelpers.h>
#include<Windows.h>

using namespace Microsoft::WRL;

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
	HRESULT Create(UINT numDesc, bool isShaderVis, D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType();
	ComPtr<ID3D12DescriptorHeap>& GetHeap();
	ID3D12DescriptorHeap* GetHeapPtr();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index);
	UINT GetLastResourceIndex();
	void IncrementLastResourceIndex(UINT valueToIncrementBy);
	UINT GetDescriptorIncrementSize();
	void CreateDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType,  size_t cbufferSize=0,UINT width = 0, UINT height = 0,UINT firstArraySlice = -1,UINT mipLevel = 0, bool isArray = false);
	void CreateDescriptor(std::wstring resName, ManagedResource& resource,
		RESOURCE_TYPE resourceType,
		TEXTURE_TYPES type = TEXTURE_TYPE_DEAULT, bool isCube = false,
		ComPtr<ID3D12Resource> uploadRes = nullptr);
	void CreateStructuredBuffer(ManagedResource& resource, ComPtr<ID3D12Device> device, UINT numElements, UINT stride, UINT bufferSize);
	void CreateRaytracingAccelerationStructureDescriptor(AccelerationStructureBuffers topLevelASBuffer);
	void UpdateRaytracingAccelerationStruct(AccelerationStructureBuffers topLevelASBuffer);
};

