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
	memcpy(volumeBufferBegin, &volData, sizeof(volData));

	ThrowIfFailed(descriptorHeap.Create(device, 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	descriptorHeap.CreateDescriptor(volumeTex, volumeDataResource, RESOURCE_TYPE_SRV, device, commandQueue,TEXTURE_TYPE_DEAULT);

	this->volumeRenderPipelineState = volumePSO;
	this->volumeRenderRootSignature = volumeRoot;
	this->volumeMesh = mesh;

}
