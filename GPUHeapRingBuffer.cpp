#include "GPUHeapRingBuffer.h"

GPUHeapRingBuffer::GPUHeapRingBuffer(ComPtr<ID3D12Device>& device)
{
	maxDesc = 1000000;
	//creating the descriptor heap that will be used like a ring buffer
	ThrowIfFailed(descriptorHeap.Create(device, maxDesc, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	head = 10000;
	tail = 10000;
	locationLastStaticResource = 9999;
	numStaticResources = 0;
}

void GPUHeapRingBuffer::AllocateStaticDescriptors(ComPtr<ID3D12Device>& device, UINT numDescriptors, DescriptorHeapWrapper otherDescHeap)
{

	auto cpuHandle = descriptorHeap.GetCPUHandle(numStaticResources);
	auto otherCPUHandle = otherDescHeap.GetCPUHandle(0);
	numStaticResources += numDescriptors;
	device->CopyDescriptorsSimple(numDescriptors, cpuHandle, otherCPUHandle, otherDescHeap.GetDescriptorHeapType());
}

void GPUHeapRingBuffer::AddDescriptor(ComPtr<ID3D12Device>& device, UINT numDescriptors, DescriptorHeapWrapper otherDescHeap, UINT frameIndex)
{
	auto cpuHandle = descriptorHeap.GetCPUHandle(tail);
	auto otherCPUHandle = otherDescHeap.GetCPUHandle(frameIndex);

	tail += numDescriptors;

	device->CopyDescriptorsSimple(numDescriptors, cpuHandle, otherCPUHandle, otherDescHeap.GetDescriptorHeapType());

	if (tail >= maxDesc) tail = locationLastStaticResource + 1;
	
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHeapRingBuffer::GetStaticDescriptorOffset()
{
	return descriptorHeap.GetGPUHandle(locationLastStaticResource+1);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHeapRingBuffer::GetDynamicResourceOffset()
{
	auto handle = descriptorHeap.GetGPUHandle(head+1);

	head++;
	if (head >= maxDesc) head = locationLastStaticResource+1;

	return handle;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHeapRingBuffer::GetBeginningStaticResourceOffset()
{
	return descriptorHeap.GetGPUHandle(0);
}

void GPUHeapRingBuffer::EndRender()
{
	head = tail;
}
