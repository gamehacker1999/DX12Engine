#include "DescriptorHeapWrapper.h"

DescriptorHeapWrapper::DescriptorHeapWrapper()
{
	memset(this, 0, sizeof(*this));
}

HRESULT DescriptorHeapWrapper::Create(ComPtr<ID3D12Device> device, UINT numDesc, bool isShaderVis, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
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

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapWrapper::GetDescriptorHeapType()
{
	return heapType;
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

UINT DescriptorHeapWrapper::GetDescriptorIncrementSize()
{
	return handleIncrementSize;
}

void DescriptorHeapWrapper::CreateDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType,
	ComPtr<ID3D12Device> device, size_t cbufferSize, UINT width, UINT height)
{
	if (resourceType == RESOURCE_TYPE_CBV)
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(cbufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource.resource.GetAddressOf())
		));

		D3D12_CONSTANT_BUFFER_VIEW_DESC sceneConstantBufferViewDesc = {};
		sceneConstantBufferViewDesc.BufferLocation = resource.resource->GetGPUVirtualAddress();
		sceneConstantBufferViewDesc.SizeInBytes = (UINT)cbufferSize;
		device->CreateConstantBufferView(&sceneConstantBufferViewDesc,
			GetCPUHandle(lastResourceIndex));

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
		resource.heapOffset = lastResourceIndex;
		
	}

	else if (resourceType == RESOURCE_TYPE_DSV)
	{

		D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
		dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsDesc.Flags = D3D12_DSV_FLAG_NONE;


		//optimized clear value for depth stencil buffer
		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.DepthStencil.Depth = 1.0f;
		depthClearValue.DepthStencil.Stencil = 0;
		depthClearValue.Format = dsDesc.Format;

		//creating the default resource heap for the depth stencil
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthClearValue,
			IID_PPV_ARGS(resource.resource.GetAddressOf())
		));

		device->CreateDepthStencilView(resource.resource.Get(), &dsDesc, GetCPUHandle(lastResourceIndex));

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		resource.heapOffset = lastResourceIndex;
	}

	else if (resourceType == RESOURCE_TYPE_UAV)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		if (resource.resource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		device->CreateUnorderedAccessView(resource.resource.Get(), nullptr, &uavDesc, GetCPUHandle(lastResourceIndex));
	}

	lastResourceIndex++;

}

void DescriptorHeapWrapper::CreateDescriptor(std::wstring resName, ManagedResource& resource, 
	RESOURCE_TYPE resourceType, ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue> commandQueue,
	TEXTURE_TYPES type)
{
	if (resourceType == RESOURCE_TYPE_SRV)
	{
		bool isCube = (type == TEXTURE_TYPE_DDS) ? true : false;
		LoadTexture(device, resource.resource, resName, commandQueue,type);
		CreateShaderResourceView(device.Get(), resource.resource.Get(), GetCPUHandle(lastResourceIndex), isCube);

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		resource.heapOffset = lastResourceIndex;
		lastResourceIndex++;
	}
}

void DescriptorHeapWrapper::CreateRaytracingAccelerationStructureDescriptor(ComPtr<ID3D12Device5> device, ManagedResource& resource, AccelerationStructureBuffers topLevelASBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = topLevelASBuffer.pResult->GetGPUVirtualAddress();
	device->CreateShaderResourceView(nullptr, &srvDesc, GetCPUHandle(lastResourceIndex));
	lastResourceIndex++;
}
