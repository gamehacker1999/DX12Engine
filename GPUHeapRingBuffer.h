#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
class GPUHeapRingBuffer
{
	UINT head;
	UINT tail;

	DescriptorHeapWrapper descriptorHeap;

public:
public: 
	GPUHeapRingBuffer(ComPtr<ID3D12Device>& device);

	//allocate static descriptors to the beginning of the ring buffer
	void AllocateStaticDescriptors(ComPtr<ID3D12Device>& device, UINT numDescriptors, DescriptorHeapWrapper otherDescHeap);

	void AddDescriptor(ManagedResource& resource,RESOURCE_TYPE resourceType);
};

