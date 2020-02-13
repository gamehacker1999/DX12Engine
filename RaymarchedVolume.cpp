#include "RaymarchedVolume.h"

RaymarchedVolume::RaymarchedVolume(std::wstring volumeTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& volumePSO, ComPtr<ID3D12RootSignature> volumeRoot, 
	ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper& mainBufferHeap)
{
	volumeBufferBegin = 0;

	//creating the volume data resource
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(volumeDataResource.GetAddressOf())
	));


	volumeDataResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&volumeBufferBegin));
	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));

	ThrowIfFailed(descriptorHeap.Create(device, 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	descriptorHeap.CreateDescriptor(volumeTex, volumeTexResource, RESOURCE_TYPE_SRV, device, commandQueue,TEXTURE_TYPE_DDS);

	this->volumeRenderPipelineState = volumePSO;
	this->volumeRenderRootSignature = volumeRoot;
	this->volumeMesh = mesh;

	position = XMFLOAT3(0, 0, 0);

}

void RaymarchedVolume::SetPosition(XMFLOAT3 pos)
{
	position = pos;
}

ComPtr<ID3D12RootSignature>& RaymarchedVolume::GetRootSignature()
{
	return volumeRenderRootSignature;
}

ComPtr<ID3D12PipelineState>& RaymarchedVolume::GetPipelineState()
{
	return volumeRenderPipelineState;
}

ComPtr<ID3D12Resource>& RaymarchedVolume::GetConstantBuffer()
{
	return volumeDataResource;
}

DescriptorHeapWrapper& RaymarchedVolume::GetDescriptorHeap()
{
	return descriptorHeap;
}

ManagedResource& RaymarchedVolume::GetVolumeTexture()
{
	return volumeTexResource;
}

std::shared_ptr<Mesh>& RaymarchedVolume::GetMesh()
{
	return volumeMesh;
}

void RaymarchedVolume::PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition, float totalTime)
{
	XMStoreFloat4x4(&volumeData.model,XMMatrixTranspose(XMMatrixTranslationFromVector(XMLoadFloat3(&position))));
	XMStoreFloat4x4(&volumeData.inverseModel, XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranslationFromVector(XMLoadFloat3(&position)))));
	volumeData.view = view;
	volumeData.proj = proj;
	volumeData.cameraPosition = camPosition;
	volumeData.focalLength = 1 / tan(0.25f * 3.14159f / 2);
	volumeData.time = totalTime;

	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));
}
