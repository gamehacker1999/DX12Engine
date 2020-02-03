#include "GPUHeapRingBuffer.h"

GPUHeapRingBuffer::GPUHeapRingBuffer(ComPtr<ID3D12Device>& device)
{
	//creating the descriptor heap that will be used like a ring buffer
	ThrowIfFailed(descriptorHeap.Create(device, UINT_MAX, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	head = 0;
	tail = 0;
}

void GPUHeapRingBuffer::AllocateStaticDescriptors(ComPtr<ID3D12Device>& device, UINT numDescriptors, DescriptorHeapWrapper otherDescHeap)
{

	auto cpuHandle = descriptorHeap.GetCPUHandle(tail);
	auto otherCPUHandle = otherDescHeap.GetCPUHandle(0);
	tail += numDescriptors;
	head += numDescriptors;
	device->CopyDescriptorsSimple(numDescriptors, cpuHandle, otherCPUHandle, otherDescHeap.GetDescriptorHeapType());
}

void GPUHeapRingBuffer::AddDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType)
{
}
