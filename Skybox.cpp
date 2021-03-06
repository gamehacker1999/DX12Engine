#include "Skybox.h"


Skybox::Skybox(std::wstring skyboxTex, std::shared_ptr<Mesh> mesh,ComPtr<ID3D12PipelineState>& skyboxPSO,
	ComPtr<ID3D12RootSignature> skyboxRoot, bool isCubeMap)
{
	/*LoadTexture(device, skyboxTexResource, skyboxTex, commandQueue, TEXTURE_TYPE_DDS);
	CreateShaderResourceView(device.Get(), skyboxTexResource.Get(), cbvSRVDescriptorHandle, true);
	cbvSRVDescriptorHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/

	ThrowIfFailed(descriptorHeap.Create(1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	descriptorHeap.CreateDescriptor(skyboxTex, skyboxTexResource, RESOURCE_TYPE_SRV,
		TEXTURE_TYPE_HDR, false, textureUpload);

	this->skyBoxPSO = skyboxPSO;
	this->skyboxRootSignature = skyboxRoot;
	skyboxMesh = mesh;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constantBufferResource.GetAddressOf())
	));

	ZeroMemory(&skyboxData, sizeof(skyboxData));

	constantBufferResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&constantBufferBegin));
	memcpy(constantBufferBegin, &skyboxData, sizeof(skyboxData));

	hasEnvironmentMaps = false;
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

DescriptorHeapWrapper& Skybox::GetDescriptorHeap()
{
	return descriptorHeap;
}

void Skybox::CreateEnvironment(ComPtr<ID3D12RootSignature>& prefilterRootSignature,
	ComPtr<ID3D12RootSignature>& brdfRootSignature,
	ComPtr<ID3D12PipelineState>& irradiencePSO, ComPtr<ID3D12PipelineState>& prefilteredMapPSO, 
	ComPtr<ID3D12PipelineState>& brdfLUTPSO, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{


	//setting the skybox descriptor heap
	hasEnvironmentMaps = true;

	dummyHeap.Create(1, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	descriptorHeap.GetCPUHandle(0);

	dummyHeap.GetCPUHandle(0);

	GetAppResources().device->CopyDescriptorsSimple(1, dummyHeap.GetCPUHandle(0), descriptorHeap.GetCPUHandle(0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ID3D12DescriptorHeap* ppHeaps[] = { dummyHeap.GetHeap().Get() };
	GetAppResources().commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	auto skyboxHandle = descriptorHeap.GetGPUHandle(0);
	environment = std::make_unique<Environment>(skyboxRootSignature, skyboxRootSignature, skyboxRootSignature,irradiencePSO,prefilteredMapPSO,
		brdfLUTPSO,dummyHeap.GetGPUHandle(0),depthStencilHandle,skyboxMesh->GetVertexBuffer(),skyboxMesh->GetIndexBuffer(),skyboxMesh->GetIndexCount());
	
}

ManagedResource& Skybox::GetSkyboxTexture()
{
	return skyboxTexResource;
}

std::shared_ptr<Mesh>& Skybox::GetMesh()
{
	return this->skyboxMesh;
}

void Skybox::PrepareForDraw(Matrix& view, Matrix& proj, Vector3& camPosition)
{
	XMStoreFloat4x4(&skyboxData.world, XMMatrixIdentity());
	skyboxData.projection = proj;
	skyboxData.view = view;
	skyboxData.cameraPos = camPosition;

	memcpy(constantBufferBegin, &skyboxData, sizeof(skyboxData));
}

DescriptorHeapWrapper Skybox::GetEnvironmentHeap()
{
	if (hasEnvironmentMaps)
	{
		return this->environment->GetSRVDescriptorHeap();
	}

	return DescriptorHeapWrapper();
}


