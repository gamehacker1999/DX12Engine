#include "RaymarchedVolume.h"

RaymarchedVolume::RaymarchedVolume(std::wstring volumeTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& volumePSO, ComPtr<ID3D12RootSignature> volumeRoot, 
	ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper& mainBufferHeap, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	volumeBufferBegin = 0;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
	//creating the volume data resource
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(volumeDataResource.GetAddressOf())
	));


	volumeDataResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&volumeBufferBegin));
	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));

	ThrowIfFailed(descriptorHeap.Create(device, 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	//descriptorHeap.CreateDescriptor(volumeTex, volumeTexResource, RESOURCE_TYPE_SRV, device, commandQueue,TEXTURE_TYPE_DDS);

	this->volumeRenderPipelineState = volumePSO;
	this->volumeRenderRootSignature = volumeRoot;
	this->volumeMesh = mesh;

	position = XMFLOAT3(0, 0, 0);/**/


	/**/std::ifstream ifile;

	ifile.open("../../Assets/Textures/Head.raw", std::ios_base::binary);

	if (!ifile)
		return;

	std::vector<UINT8> values(256 * 256 * 256*4);

	ifile.read((char*)&values[0], 256 * 256 * 256*4);

	ifile.close();

	//creating the texture resource description
	auto texDesc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256, 256);
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(volumeTexResource.resource.GetAddressOf())
	));

	volumeTexResource.currentState = D3D12_RESOURCE_STATE_COPY_DEST;
	volumeTexResource.resourceType = RESOURCE_TYPE_SRV;

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(volumeTexResource.resource.Get(), 0, 1);

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	//create the gpu upload buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(textureUpload.GetAddressOf())
	));

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &values[0];
	textureData.RowPitch = 256*2;
	textureData.SlicePitch = textureData.RowPitch * 256*2;

	UpdateSubresources<1>(commandList.Get(), volumeTexResource.resource.Get(), textureUpload.Get(), 0, 0, 1, &textureData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(volumeTexResource.resource.Get(), volumeTexResource.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);

	volumeTexResource.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//create an SRV that will use this texture
	CreateShaderResourceView(device.Get(), volumeTexResource.resource.Get(), descriptorHeap.GetCPUHandle(0), false);


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
	XMFLOAT4X4 viewTrans;
	XMStoreFloat4x4(&viewTrans, XMMatrixTranspose(XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&volumeData.viewInv, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&viewTrans))));
	volumeData.proj = proj;
	volumeData.cameraPosition = camPosition;
	volumeData.focalLength = 1 / tan(0.25f * 3.14159f / 2);
	volumeData.time = totalTime;

	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));
}

void RaymarchedVolume::Render(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, 
	std::shared_ptr<GPUHeapRingBuffer> ringBuffer)
{
	commandList->SetPipelineState(GetPipelineState().Get());
	commandList->SetGraphicsRootSignature(GetRootSignature().Get());
	commandList->SetGraphicsRootConstantBufferView(0, GetConstantBuffer()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, ringBuffer->GetDescriptorHeap().GetGPUHandle(volumeTextureIndex));
	auto vBuff = GetMesh()->GetVertexBuffer();
	auto iBuff = GetMesh()->GetIndexBuffer();
	commandList->IASetVertexBuffers(0, 1, &vBuff);
	commandList->IASetIndexBuffer(&iBuff);
	commandList->DrawIndexedInstanced(GetMesh()->GetIndexCount(), 1, 0, 0, 0);
}
