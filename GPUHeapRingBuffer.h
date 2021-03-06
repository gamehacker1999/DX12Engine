#pragma once
#include"DX12Helper.h"
#include"DescriptorHeapWrapper.h"
class GPUHeapRingBuffer
{
	UINT head;
	UINT tail;
	UINT numStaticResources;
	UINT locationLastStaticResource;
	UINT currentDynamicResource;
	UINT maxDesc;

	DescriptorHeapWrapper descriptorHeap;

public:
public: 
	GPUHeapRingBuffer();

	//allocate static descriptors to the beginning of the ring buffer
	void AllocateStaticDescriptors(UINT numDescriptors, DescriptorHeapWrapper& otherDescHeap);

	void AddDescriptor(UINT numDescriptors, DescriptorHeapWrapper& otherDescHeap, UINT frameIndex);

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetStaticDescriptorOffset();

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetDynamicResourceOffset();

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetBeginningStaticResourceOffset();

	DescriptorHeapWrapper& GetDescriptorHeap();

	void IncrementNumStaticResources(UINT num);

	UINT GetNumStaticResources();

	void EndRender();
};

