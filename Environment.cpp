#include "Environment.h"

Environment::Environment(ComPtr<ID3D12RootSignature> irradianceRootSignature, ComPtr<ID3D12RootSignature> prefilteredRootSignature, 
	ComPtr<ID3D12RootSignature> brdfRootSignature,
	ComPtr<ID3D12PipelineState> irradiencePSO, ComPtr<ID3D12PipelineState> prefilteredMapPSO,
	ComPtr<ID3D12PipelineState> brdfLUTPSO, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList,
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle,
	D3D12_VERTEX_BUFFER_VIEW skyboxCube, D3D12_INDEX_BUFFER_VIEW indexBuffer, UINT indexCount)
{
    this->irradiencePSO = irradiencePSO;
    this->prefilteredMapPSO = prefilteredMapPSO;
    this->brdfLUTPSO = brdfLUTPSO;
    this->prefilteredRootSignature = prefilteredRootSignature;
    this->irradianceRootSignature = irradianceRootSignature;
    this->brdfRootSignature = brdfRootSignature;
    ThrowIfFailed(srvDescriptorHeap.Create(device, 3, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    ThrowIfFailed(rtvDescriptorHeap.Create(device, 43, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

    ThrowIfFailed(dsvDescriptorHeap.Create(device, 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(environmentData) * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constantBufferResource.GetAddressOf())
	));

	environmentDataBegin = 0;


	XMFLOAT4X4 cubePosxView;
	XMFLOAT4X4 cubeNegxView;
	XMFLOAT4X4 cubePoszView;
	XMFLOAT4X4 cubeNegzView;
	XMFLOAT4X4 cubePosyView;
	XMFLOAT4X4 cubeNegyView;

	//postive x face of cube
	XMStoreFloat4x4(&cubePosxView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
	cubemapViews.emplace_back(cubePosxView);

	//negative x face of cube
	XMStoreFloat4x4(&cubeNegxView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
	cubemapViews.emplace_back(cubeNegxView);

	//postive y face of cube
	XMStoreFloat4x4(&cubePosyView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f))));
	cubemapViews.emplace_back(cubePosyView);

	//negative y face of cube
	XMStoreFloat4x4(&cubeNegyView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))));
	cubemapViews.emplace_back(cubeNegyView);

	//postive z face of cube
	XMStoreFloat4x4(&cubePoszView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
	cubemapViews.emplace_back(cubePoszView);

	//negative z face of cube
	XMStoreFloat4x4(&cubeNegzView, XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
	cubemapViews.emplace_back(cubeNegzView);

	//setting the cubemap projection
	XMStoreFloat4x4(&cubemapProj, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.0f, 0.1f, 10000.f)));

	ZeroMemory(&environmentData, sizeof(environmentData));

	XMStoreFloat4x4(&environmentData.world, XMMatrixIdentity());
	environmentData.view = cubemapViews[2];
	environmentData.projection = cubemapProj;
	environmentData.cameraPos = XMFLOAT3(0, 0, 0);

	constantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&environmentDataBegin));
	memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));

	this->skyboxCube = skyboxCube;
	this->skyboxIndexBuffer = indexBuffer;
	this->indexCount = indexCount;

	cube = std::make_shared<Mesh>("../../Assets/Models/cube.obj", device, commandList);

	CreateIrradianceMap(device,commandList,skyboxHandle,depthStencilHandle);
	CreatePrefilteredEnvironmentMap(device, commandList, skyboxHandle,depthStencilHandle);
	CreateBRDFLut(device, commandList, depthStencilHandle);


}

Environment::~Environment()
{
}

void Environment::CreateIrradianceMap(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList,CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{
	D3D12_RESOURCE_DESC irradienaceTextureDesc = {};
	irradienaceTextureDesc.DepthOrArraySize = 6;
	irradienaceTextureDesc.MipLevels = 1;
	irradienaceTextureDesc.Width = (UINT64)64;
	irradienaceTextureDesc.Height = (UINT64)64;
	irradienaceTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	irradienaceTextureDesc.SampleDesc.Count = 1;
	irradienaceTextureDesc.SampleDesc.Quality = 0;
	irradienaceTextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	irradienaceTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	D3D12_CLEAR_VALUE rtvClearVal = {};
	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	rtvClearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&irradienaceTextureDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(irradienceMapTexture.resource.GetAddressOf())));

	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the irradiance srv
	srvDescriptorHeap.CreateDescriptor(irradienceMapTexture, RESOURCE_TYPE_SRV, device, 0, 64, 64, 0, 1);

	viewPort = {};
	viewPort.Height = 64.f;
	viewPort.Width = 64.f;
	viewPort.MaxDepth = 1.f;
	viewPort.MinDepth = 0.0f;
	viewPort.TopLeftX = 0.f;
	viewPort.TopLeftY = 0.0f;

	scissorRect = {};
	scissorRect.bottom = 64;
	scissorRect.right = 64;
	scissorRect.left = 0;
	scissorRect.top = 0;

	commandList->SetGraphicsRootSignature(irradianceRootSignature.Get());
	commandList->SetPipelineState(irradiencePSO.Get());
	commandList->RSSetViewports(1, &viewPort);
	commandList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentTexturesData, constantBufferResource->GetGPUVirtualAddress());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(irradienceMapTexture.resource.Get(), irradienceMapTexture.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET));
	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;


	for (int i = 0; i < 6; i++)
	{
		rtvDescriptorHeap.CreateDescriptor(irradienceMapTexture, RESOURCE_TYPE_RTV, device, 0, 64, 64, i, 0);
		commandList->ClearRenderTargetView(rtvDescriptorHeap.GetCPUHandle(irradienceMapTexture.heapOffset), clearColor, 0, 0);
		commandList->OMSetRenderTargets(1, &rtvDescriptorHeap.GetCPUHandle(irradienceMapTexture.heapOffset), FALSE, &depthStencilHandle);
		commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

		XMStoreFloat4x4(&environmentData.world, XMMatrixIdentity());
		environmentData.view = cubemapViews[i];
		environmentData.projection = cubemapProj;
		environmentData.cameraPos = XMFLOAT3(0, 0, 0);

		memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));


		commandList->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, skyboxHandle);
		commandList->SetGraphicsRoot32BitConstant(EnvironmentRootIndices::EnvironmentFaceIndices, i, 0);

		commandList->DrawInstanced(3, 1, 0, 0);
	}

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(irradienceMapTexture.resource.Get(), irradienceMapTexture.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Environment::CreateBRDFLut(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{

	D3D12_RESOURCE_DESC integrationLUTTexture = {};
	integrationLUTTexture.DepthOrArraySize = 1;
	integrationLUTTexture.MipLevels = 0;
	integrationLUTTexture.Width = 64;
	integrationLUTTexture.Height = 64;
	integrationLUTTexture.SampleDesc.Count = 1;
	integrationLUTTexture.SampleDesc.Quality = 0;
	integrationLUTTexture.Format = DXGI_FORMAT_R32G32_FLOAT;
	integrationLUTTexture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	integrationLUTTexture.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	D3D12_CLEAR_VALUE rtvClearVal = {};
	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	rtvClearVal.Format = DXGI_FORMAT_R32G32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&integrationLUTTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &rtvClearVal, IID_PPV_ARGS(environmentBRDFLUT.resource.GetAddressOf())));

	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the srv
	srvDescriptorHeap.CreateDescriptor(environmentBRDFLUT, RESOURCE_TYPE_SRV, device, 0, 0, 0, 0, 1);

	viewPort = {};
	viewPort.Height = 64;
	viewPort.Width = 64;
	viewPort.MaxDepth = 1.f;
	viewPort.MinDepth = 0.0f;
	viewPort.TopLeftX = 0.f;
	viewPort.TopLeftY = 0.0f;

	scissorRect = {};
	scissorRect.bottom = 64;
	scissorRect.right = 64;
	scissorRect.left = 0;
	scissorRect.top = 0;

	commandList->SetGraphicsRootSignature(brdfRootSignature.Get());
	commandList->SetPipelineState(brdfLUTPSO.Get());
	commandList->RSSetViewports(1, &viewPort);
	commandList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(environmentBRDFLUT.resource.Get(), environmentBRDFLUT.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET));
	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	rtvDescriptorHeap.CreateDescriptor(environmentBRDFLUT, RESOURCE_TYPE_RTV, device, 0, 64, 64, 0, 0);
	commandList->ClearRenderTargetView(environmentBRDFLUT.rtvCPUHandle, clearColor, 0, 0);
	commandList->OMSetRenderTargets(1, &environmentBRDFLUT.rtvCPUHandle, FALSE, &depthStencilHandle);
	commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

	commandList->DrawInstanced(3, 1, 0, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(environmentBRDFLUT.resource.Get(), environmentBRDFLUT.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Environment::CreatePrefilteredEnvironmentMap(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{

	D3D12_RESOURCE_DESC prefilterMapDesc = {};
	prefilterMapDesc.DepthOrArraySize = 6;
	prefilterMapDesc.MipLevels = 5;
	prefilterMapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	prefilterMapDesc.Width = 128;
	prefilterMapDesc.Height = 128;
	prefilterMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	prefilterMapDesc.SampleDesc.Count = 1;
	prefilterMapDesc.SampleDesc.Quality = 0;
	prefilterMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	D3D12_CLEAR_VALUE rtvClearVal = {};
	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	rtvClearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&prefilterMapDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &rtvClearVal, IID_PPV_ARGS(prefilteredMapTextures.resource.GetAddressOf())));

	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the srv
	srvDescriptorHeap.CreateDescriptor(prefilteredMapTextures, RESOURCE_TYPE_SRV, device, 0, 0, 0, 0, 5);

	commandList->SetGraphicsRootSignature(prefilteredRootSignature.Get());
	commandList->SetPipelineState(prefilteredMapPSO.Get());

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(prefilteredMapTextures.resource.Get(), prefilteredMapTextures.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET));
	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentTexturesData, constantBufferResource->GetGPUVirtualAddress());


	for (UINT mip = 0; mip < 5; mip++)
	{
		double width = 128 * pow(0.5, mip);
		double height = 128 * pow(0.5, mip);

		viewPort = {};
		viewPort.Height = (float)height;
		viewPort.Width = (float)width;
		viewPort.MaxDepth = 1.f;
		viewPort.MinDepth = 0.0f;
		viewPort.TopLeftX = 0.f;
		viewPort.TopLeftY = 0.0f;

		scissorRect = {};
		scissorRect.bottom = (long)width;
		scissorRect.right = (LONG)height;
		scissorRect.left = 0;
		scissorRect.top = 0;

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		float roughness = (float)mip / (5.f - 1.f);

		for (int i = 0; i < 6; i++)
		{
			rtvDescriptorHeap.CreateDescriptor(prefilteredMapTextures, RESOURCE_TYPE_RTV, device, 0, width, height, i, mip);
			commandList->ClearRenderTargetView(rtvDescriptorHeap.GetCPUHandle(prefilteredMapTextures.heapOffset), clearColor, 0, 0);
			commandList->OMSetRenderTargets(1, &rtvDescriptorHeap.GetCPUHandle(prefilteredMapTextures.heapOffset), FALSE, &depthStencilHandle);
			commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

			XMStoreFloat4x4(&environmentData.world, XMMatrixIdentity());
			//environmentData.view = cubemapViews[i];
			environmentData.projection = cubemapProj;
			environmentData.cameraPos = XMFLOAT3(0, 3, 0);
			memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));

			auto address = constantBufferResource->GetGPUVirtualAddress();
			commandList->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, skyboxHandle);
			commandList->SetGraphicsRoot32BitConstants(EnvironmentRootIndices::EnvironmentRoughness, 1,&roughness, 0);
			commandList->SetGraphicsRoot32BitConstants(EnvironmentRootIndices::EnvironmentFaceIndices, 1, &i, 0);

			commandList->DrawInstanced(3, 1, 0, 0);
		}
	}


	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(prefilteredMapTextures.resource.Get(), prefilteredMapTextures.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}



DescriptorHeapWrapper Environment::GetSRVDescriptorHeap()
{
	return srvDescriptorHeap;
}
