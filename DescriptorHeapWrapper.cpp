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

ID3D12DescriptorHeap* DescriptorHeapWrapper::GetHeapPtr()
{
	return GetHeap().Get();
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

UINT DescriptorHeapWrapper::GetLastResourceIndex()
{
	return lastResourceIndex;
}

void DescriptorHeapWrapper::IncrementLastResourceIndex(UINT valueToIncrementBy)
{
	lastResourceIndex += valueToIncrementBy;
}

UINT DescriptorHeapWrapper::GetDescriptorIncrementSize()
{
	return handleIncrementSize;
}

void DescriptorHeapWrapper::CreateDescriptor(ManagedResource& resource, RESOURCE_TYPE resourceType,
	ComPtr<ID3D12Device> device, size_t cbufferSize, UINT width, UINT height, UINT firstArraySlice, UINT mipLevel)
{


	auto uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbufferSize);
	if (resourceType == RESOURCE_TYPE_CBV)
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
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
		resource.cbvGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.cbvCPUHandle = GetCPUHandle(lastResourceIndex);
		
	}

	else if (resourceType == RESOURCE_TYPE_DSV)
	{

		D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
		dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(resource.resource.Get(), &dsDesc, GetCPUHandle(lastResourceIndex));

		resource.resourceType = resourceType;

		resource.heapOffset = lastResourceIndex;

		resource.dsvGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.dsvCPUHandle = GetCPUHandle(lastResourceIndex);
	}

	else if (resourceType == RESOURCE_TYPE_UAV)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		if (resource.resource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = mipLevel;
		}
		uavDesc.Format = resource.resource->GetDesc().Format;
		device->CreateUnorderedAccessView(resource.resource.Get(), nullptr, &uavDesc, GetCPUHandle(lastResourceIndex));
		resource.uavGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.uavCPUHandle = GetCPUHandle(lastResourceIndex);
		resource.heapOffset = lastResourceIndex;

	}

	else if (resourceType == RESOURCE_TYPE_SRV)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		if (resource.resource->GetDesc().DepthOrArraySize == 6)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = mipLevel;
	
		}

		else if (resource.resource->GetDesc().DepthOrArraySize == 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = mipLevel;
			srvDesc.Texture2D.MostDetailedMip = 0;
		}

		auto resourceDesc = resource.resource->GetDesc();
		srvDesc.Format = resourceDesc.Format;
		if (resourceDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
		{
			srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		device->CreateShaderResourceView(resource.resource.Get(), &srvDesc, GetCPUHandle(lastResourceIndex));
		resource.srvGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.srvCPUHandle = GetCPUHandle(lastResourceIndex);
		resource.heapOffset = lastResourceIndex;

	}

	else if (resourceType == RESOURCE_TYPE_RTV)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		auto resourceDesc = resource.resource->GetDesc();


		if (resourceDesc.DepthOrArraySize > 1)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.MipSlice = mipLevel;
			rtvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
		}		

		else if (resourceDesc.DepthOrArraySize == 1)
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		rtvDesc.Format = resourceDesc.Format;
		device->CreateRenderTargetView(resource.resource.Get(), &rtvDesc, GetCPUHandle(lastResourceIndex));

		resource.rtvGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.rtvCPUHandle = GetCPUHandle(lastResourceIndex);
		resource.heapOffset = lastResourceIndex;

	}

	lastResourceIndex++;

}

void DescriptorHeapWrapper::CreateDescriptor(std::wstring resName, ManagedResource& resource, 
	RESOURCE_TYPE resourceType, ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue,
	TEXTURE_TYPES type, bool isCube, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> uploadRes)
{
	if (resourceType == RESOURCE_TYPE_SRV)
	{
		LoadTexture(device, resource.resource, resName, commandQueue, commandList, uploadRes.Get(), type);
		CreateShaderResourceView(device.Get(), resource.resource.Get(), GetCPUHandle(lastResourceIndex), isCube);

		auto desc = resource.resource->GetDesc();

		resource.width = desc.Width;
		resource.height = desc.Height;
		resource.mipLevels = desc.MipLevels;

		resource.resourceType = resourceType;
		resource.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		resource.heapOffset = lastResourceIndex;
		lastResourceIndex++;
		resource.srvGPUHandle = GetGPUHandle(lastResourceIndex);
		resource.srvCPUHandle = GetCPUHandle(lastResourceIndex);
	}
}

void DescriptorHeapWrapper::CreateStructuredBuffer(ManagedResource& resource, ComPtr<ID3D12Device> device, 
	UINT numElements, UINT stride, UINT bufferSize)
{

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(resource.resource.GetAddressOf())
	));

	D3D12_SHADER_RESOURCE_VIEW_DESC structureBufferDesc = {};
	structureBufferDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	structureBufferDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	structureBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	structureBufferDesc.Buffer.FirstElement = 0;
	structureBufferDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	structureBufferDesc.Buffer.NumElements = numElements;
	structureBufferDesc.Buffer.StructureByteStride = stride;

	device->CreateShaderResourceView(resource.resource.Get(), &structureBufferDesc, GetCPUHandle(lastResourceIndex));

	resource.resourceType = RESOURCE_TYPE_SRV;
	resource.currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
	resource.heapOffset = lastResourceIndex;

	resource.srvGPUHandle = GetGPUHandle(lastResourceIndex);
	resource.srvCPUHandle = GetCPUHandle(lastResourceIndex);

	lastResourceIndex++;
}

void DescriptorHeapWrapper::CreateRaytracingAccelerationStructureDescriptor(ComPtr<ID3D12Device5> device, AccelerationStructureBuffers topLevelASBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = topLevelASBuffer.pResult->GetGPUVirtualAddress();
	device->CreateShaderResourceView(nullptr, &srvDesc, GetCPUHandle(lastResourceIndex));

	topLevelASBuffer.srvIndex = lastResourceIndex;

	lastResourceIndex++;
}

void DescriptorHeapWrapper::UpdateRaytracingAccelerationStruct(ComPtr<ID3D12Device5> device, AccelerationStructureBuffers topLevelASBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = topLevelASBuffer.pResult->GetGPUVirtualAddress();
	device->CreateShaderResourceView(nullptr, &srvDesc, GetCPUHandle(topLevelASBuffer.srvIndex));

	//topLevelASBuffer.srvIndex = lastResourceIndex;
}
