#include "RaymarchedVolume.h"

RaymarchedVolume::RaymarchedVolume(std::wstring volumeTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& volumePSO, ComPtr<ID3D12RootSignature> volumeRoot, 
DescriptorHeapWrapper& mainBufferHeap)
{
	volumeBufferBegin = 0;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
	//creating the volume data resource
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(volumeDataResource.GetAddressOf())
	));


	volumeDataResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&volumeBufferBegin));
	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));

	ThrowIfFailed(descriptorHeap.Create(1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	//descriptorHeap.CreateDescriptor(volumeTex, volumeTexResource, RESOURCE_TYPE_SRV, device, commandQueue,TEXTURE_TYPE_DDS);

	this->volumeRenderPipelineState = volumePSO;
	this->volumeRenderRootSignature = volumeRoot;
	this->volumeMesh = mesh;

	position = Vector3(0, 0, 0);/**/


	/**/std::ifstream ifile;

	ifile.open("../../Assets/Textures/Head.raw", std::ios_base::binary);

	if (!ifile)
		return;

	std::vector<UINT8> values(256 * 256 * 256*4);

	ifile.read((char*)&values[0], 256 * 256 * 256*4);

	ifile.close();

	//creating the texture resource description
	auto texDesc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256, 256);
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
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
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
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

	UpdateSubresources<1>(GetAppResources().commandList.Get(), volumeTexResource.resource.Get(), textureUpload.Get(), 0, 0, 1, &textureData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(volumeTexResource.resource.Get(), volumeTexResource.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);

	volumeTexResource.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//create an SRV that will use this texture
	CreateShaderResourceView(GetAppResources().device.Get(), volumeTexResource.resource.Get(), descriptorHeap.GetCPUHandle(0), false);


}

void RaymarchedVolume::SetPosition(Vector3 pos)
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

void RaymarchedVolume::PrepareForDraw(Matrix view, Matrix proj, Vector3 camPosition, float totalTime)
{
	XMFLOAT3 scale(00, 00, 00);
	XMStoreFloat4x4(&volumeData.model,XMMatrixTranspose(XMMatrixScalingFromVector(XMLoadFloat3(&scale))* XMMatrixTranslationFromVector(XMLoadFloat3(&position))));
	XMStoreFloat4x4(&volumeData.inverseModel, XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranslationFromVector(XMLoadFloat3(&position)))));
	volumeData.view = view;
	Matrix viewTrans;
	XMStoreFloat4x4(&viewTrans, XMMatrixTranspose(XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&volumeData.viewInv, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&viewTrans))));
	volumeData.proj = proj;
	volumeData.cameraPosition = camPosition;
	volumeData.focalLength = 1 / tan(0.25f * 3.14159f / 2);
	volumeData.time = totalTime;

	memcpy(volumeBufferBegin, &volumeData, sizeof(volumeData));
}

void RaymarchedVolume::Render(
	std::shared_ptr<GPUHeapRingBuffer>& ringBuffer)
{
	GetAppResources().commandList->SetPipelineState(GetPipelineState().Get());
	GetAppResources().commandList->SetGraphicsRootSignature(GetRootSignature().Get());
	GetAppResources().commandList->SetGraphicsRootConstantBufferView(0, GetConstantBuffer()->GetGPUVirtualAddress());
	GetAppResources().commandList->SetGraphicsRootDescriptorTable(1, ringBuffer->GetDescriptorHeap().GetGPUHandle(volumeTextureIndex));
	auto vBuff = GetMesh()->GetVertexBuffer();
	auto iBuff = GetMesh()->GetIndexBuffer();
	GetAppResources().commandList->IASetVertexBuffers(0, 1, &vBuff);
	GetAppResources().commandList->IASetIndexBuffer(&iBuff);
	GetAppResources().commandList->DrawIndexedInstanced(GetMesh()->GetIndexCount(), 1, 0, 0, 0);
}
