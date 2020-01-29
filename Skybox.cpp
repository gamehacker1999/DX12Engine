#include "Skybox.h"


Skybox::Skybox(std::wstring skyboxTex, std::shared_ptr<Mesh> mesh, ComPtr<ID3D12PipelineState>& skyboxPSO,
	ComPtr<ID3D12RootSignature> skyboxRoot, ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue,
	DescriptorHeapWrapper mainBufferHeap)
{
	/*LoadTexture(device, skyboxTexResource, skyboxTex, commandQueue, TEXTURE_TYPE_DDS);
	CreateShaderResourceView(device.Get(), skyboxTexResource.Get(), cbvSRVDescriptorHandle, true);
	cbvSRVDescriptorHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/

	mainBufferHeap.CreateDescriptor(skyboxTex, skyboxTexResource, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DDS);

	this->skyBoxPSO = skyboxPSO;
	this->skyboxRootSignature = skyboxRoot;
	this->skyboxMesh = mesh;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024*64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constantBufferResource.GetAddressOf())
	));

	ZeroMemory(&skyboxData, sizeof(skyboxData));

	constantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&constantBufferBegin));
	memcpy(constantBufferBegin, &skyboxData, sizeof(skyboxData));
}

ComPtr<ID3D12RootSignature>& Skybox::GetRootSignature()
{
	return skyboxRootSignature;
}

ComPtr<ID3D12PipelineState>& Skybox::GetPipelineState()
{
	return skyBoxPSO;
}

ComPtr<ID3D12Resource>& Skybox::GetConstantBuffer()
{
	return constantBufferResource;
}

std::shared_ptr<Mesh>& Skybox::GetMesh()
{
	return this->skyboxMesh;
}

void Skybox::PrepareForDraw(XMFLOAT4X4 view, XMFLOAT4X4 proj, XMFLOAT3 camPosition)
{
	XMStoreFloat4x4(&skyboxData.world, XMMatrixIdentity());
	skyboxData.projection = proj;
	skyboxData.view = view;
	skyboxData.cameraPos = camPosition;

	memcpy(constantBufferBegin, &skyboxData, sizeof(skyboxData));
}


