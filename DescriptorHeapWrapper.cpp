#include "DescriptorHeapWrapper.h"

DescriptorHeapWrapper::DescriptorHeapWrapper()
{
	memset(this, 0, sizeof(*this));
}

HRESULT DescriptorHeapWrapper::Create(ComPtr<ID3D12Device>& device, UINT numDesc, bool isShaderVis, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	this->isShaderVisible = isShaderVis;
	this->numDescriptor = numDesc;
	this->heapType = heapType;
	this->lastResourceIndex = 0;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = this->numDescriptor;
	heapDesc.Type = this->heapType;
	heapDesc.Flags = (isShaderVis ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf())));

	cpuHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	this->handleIncrementSize = device->GetDescriptorHandleIncrementSize(heapDesc.Type);

	return S_OK;
}

ComPtr<ID3D12DescriptorHeap>& DescriptorHeapWrapper::GetHeap()
{
	return descriptorHeap;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapWrapper::GetCPUHandle(UINT index)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE offsettedDescHandle(cpuHandle, index, handleIncrementSize);
	return offsettedDescHandle;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapWrapper::GetGPUHandle(UINT index)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE offsettedDescHandle(gpuHandle, index, handleIncrementSize);
	return offsettedDescHandle;
}

void DescriptorHeapWrapper::CreateDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType,ComPtr<ID3D12Device>& device)
{
	if (resourceType == RESOURCE_TYPE_CBV)
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(1024*64)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource.resource.GetAddressOf())
		));

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
		resource.heapOffset = lastResourceIndex;
		lastResourceIndex++;
	}

}

void DescriptorHeapWrapper::CreateDescriptor(std::wstring resName, ManagedResource& resource, RESOURCE_TYPE resourceType, ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue> commandQueue)
{
	if (resourceType == RESOURCE_TYPE_SRV)
	{
		LoadTexture(device, resource.resource, resName, commandQueue);
		CreateShaderResourceView(device.Get(), resource.resource.Get(), this->cpuHandle, false);

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		resource.heapOffset = lastResourceIndex;
		lastResourceIndex++;
	}
}
