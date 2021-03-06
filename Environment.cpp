#include "Environment.h"

Environment::Environment(ComPtr<ID3D12RootSignature>& irradianceRootSignature, ComPtr<ID3D12RootSignature>& prefilteredRootSignature, ComPtr<ID3D12RootSignature>& brdfRootSignature,
	ComPtr<ID3D12PipelineState>& irradiencePSO, ComPtr<ID3D12PipelineState>& prefilteredMapPSO,
	ComPtr<ID3D12PipelineState>& brdfLUTPSO,
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle,
	D3D12_VERTEX_BUFFER_VIEW skyboxCube, D3D12_INDEX_BUFFER_VIEW indexBuffer, UINT indexCount)
{
    this->irradiencePSO = irradiencePSO;
    this->prefilteredMapPSO = prefilteredMapPSO;
    this->brdfLUTPSO = brdfLUTPSO;
    this->prefilteredRootSignature = prefilteredRootSignature;
    this->irradianceRootSignature = irradianceRootSignature;
    this->brdfRootSignature = brdfRootSignature;
    ThrowIfFailed(srvDescriptorHeap.Create( 3, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    ThrowIfFailed(rtvDescriptorHeap.Create( 43, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

    ThrowIfFailed(dsvDescriptorHeap.Create(1, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(environmentData) * 64);
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
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

	constantBufferResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&environmentDataBegin));
	memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));

	this->skyboxCube = skyboxCube;
	this->skyboxIndexBuffer = indexBuffer;
	this->indexCount = indexCount;

	cube = std::make_shared<Mesh>("../../Assets/Models/cube.obj");

	CreateIrradianceMap(skyboxHandle,depthStencilHandle);
	CreatePrefilteredEnvironmentMap(skyboxHandle,depthStencilHandle);
	CreateBRDFLut(depthStencilHandle);


}

Environment::~Environment()
{
}

void Environment::CreateIrradianceMap(CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{
	D3D12_RESOURCE_DESC irradienaceTextureDesc = {};
	irradienaceTextureDesc.DepthOrArraySize = 6;
	irradienaceTextureDesc.MipLevels = 1;
	irradienaceTextureDesc.Width = 64;
	irradienaceTextureDesc.Height = 64;
	irradienaceTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	irradienaceTextureDesc.SampleDesc.Count = 1;
	irradienaceTextureDesc.SampleDesc.Quality = 0;
	irradienaceTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	irradienaceTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	D3D12_CLEAR_VALUE rtvClearVal = {};
	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	rtvClearVal.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&irradienaceTextureDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(irradienceMapTexture.resource.GetAddressOf())));

	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the irradiance srv
	srvDescriptorHeap.CreateDescriptor(irradienceMapTexture, RESOURCE_TYPE_SRV, 0, 512, 512, 0, 1);

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

	GetAppResources().commandList->SetGraphicsRootSignature(irradianceRootSignature.Get());
	GetAppResources().commandList->SetPipelineState(irradiencePSO.Get());
	GetAppResources().commandList->RSSetViewports(1, &viewPort);
	GetAppResources().commandList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	GetAppResources().commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	GetAppResources().commandList->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentTexturesData, constantBufferResource->GetGPUVirtualAddress());

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(irradienceMapTexture.resource.Get(), irradienceMapTexture.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;


	for (int i = 0; i < 6; i++)
	{
		rtvDescriptorHeap.CreateDescriptor(irradienceMapTexture, RESOURCE_TYPE_RTV, 0, 64, 64, i, 0);
		auto rtvCPUHandle = irradienceMapTexture.rtvCPUHandle;
		GetAppResources().commandList->ClearRenderTargetView(rtvCPUHandle, clearColor, 0, 0);
		GetAppResources().commandList->OMSetRenderTargets(1, &rtvCPUHandle, FALSE, &depthStencilHandle);
		GetAppResources().commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

		XMStoreFloat4x4(&environmentData.world, XMMatrixIdentity());
		environmentData.view = cubemapViews[i];
		environmentData.projection = cubemapProj;
		environmentData.cameraPos = XMFLOAT3(0, 0, 0);

		memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));

		GetAppResources().commandList->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, skyboxHandle);
		GetAppResources().commandList->SetGraphicsRoot32BitConstant(EnvironmentRootIndices::EnvironmentFaceIndices, i, 0);
		GetAppResources().commandList->DrawInstanced(3, 1, 0, 0);
	}

	transition = CD3DX12_RESOURCE_BARRIER::Transition(irradienceMapTexture.resource.Get(), irradienceMapTexture.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	irradienceMapTexture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Environment::CreateBRDFLut(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
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

	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(&GetAppResources().defaultHeapType, D3D12_HEAP_FLAG_NONE,
		&integrationLUTTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &rtvClearVal, IID_PPV_ARGS(environmentBRDFLUT.resource.GetAddressOf())));

	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the srv
	srvDescriptorHeap.CreateDescriptor(environmentBRDFLUT, RESOURCE_TYPE_SRV, 0, 0, 0, 0, 1);

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

	GetAppResources().commandList->SetGraphicsRootSignature(brdfRootSignature.Get());
	GetAppResources().commandList->SetPipelineState(brdfLUTPSO.Get());
	GetAppResources().commandList->RSSetViewports(1, &viewPort);
	GetAppResources().commandList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	GetAppResources().commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(environmentBRDFLUT.resource.Get(), environmentBRDFLUT.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	rtvDescriptorHeap.CreateDescriptor(environmentBRDFLUT, RESOURCE_TYPE_RTV, 0, 64, 64, 0, 0);
	GetAppResources().commandList->ClearRenderTargetView(environmentBRDFLUT.rtvCPUHandle, clearColor, 0, 0);
	GetAppResources().commandList->OMSetRenderTargets(1, &environmentBRDFLUT.rtvCPUHandle, FALSE, &depthStencilHandle);
	GetAppResources().commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);
	GetAppResources().commandList->DrawInstanced(3, 1, 0, 0);

	transition = CD3DX12_RESOURCE_BARRIER::Transition(environmentBRDFLUT.resource.Get(), environmentBRDFLUT.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	environmentBRDFLUT.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Environment::CreatePrefilteredEnvironmentMap(CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxHandle, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{

	D3D12_RESOURCE_DESC prefilterMapDesc = {};
	prefilterMapDesc.DepthOrArraySize = 6;
	prefilterMapDesc.MipLevels = 5;
	prefilterMapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	prefilterMapDesc.Width = 2048;
	prefilterMapDesc.Height = 2048;
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
	rtvClearVal.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().defaultHeapType, 
		D3D12_HEAP_FLAG_NONE,
		&prefilterMapDesc, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 
		&rtvClearVal, 
		IID_PPV_ARGS(prefilteredMapTextures.resource.GetAddressOf())));

	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//creating the srv
	srvDescriptorHeap.CreateDescriptor(prefilteredMapTextures, RESOURCE_TYPE_SRV,0, 0, 0, 0, 5);

	GetAppResources().commandList->SetGraphicsRootSignature(prefilteredRootSignature.Get());
	GetAppResources().commandList->SetPipelineState(prefilteredMapPSO.Get());

	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	GetAppResources().commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(prefilteredMapTextures.resource.Get(), prefilteredMapTextures.currentState, D3D12_RESOURCE_STATE_RENDER_TARGET);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	GetAppResources().commandList->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentTexturesData, constantBufferResource->GetGPUVirtualAddress());


	for (UINT mip = 0; mip < 5; mip++)
	{
		double width = 2048 * pow(0.5, mip);
		double height = 2048 * pow(0.5, mip);

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

		GetAppResources().commandList->RSSetViewports(1, &viewPort);
		GetAppResources().commandList->RSSetScissorRects(1, &scissorRect);

		float roughness = (float)mip / (5.f - 1.f);

		for (int i = 0; i < 6; i++)
		{
			rtvDescriptorHeap.CreateDescriptor(prefilteredMapTextures, RESOURCE_TYPE_RTV, 0, width, height, i, mip);
			auto rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(prefilteredMapTextures.heapOffset);
			GetAppResources().commandList->ClearRenderTargetView(rtvDescriptorHeap.GetCPUHandle(prefilteredMapTextures.heapOffset), clearColor, 0, 0);
			GetAppResources().commandList->OMSetRenderTargets(1, &rtvCPUHandle, FALSE, nullptr);
			//commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

			XMStoreFloat4x4(&environmentData.world, XMMatrixIdentity());
			//environmentData.view = cubemapViews[i];
			environmentData.projection = cubemapProj;
			environmentData.cameraPos = XMFLOAT3(0, 3, 0);
			memcpy(environmentDataBegin, &environmentData, sizeof(environmentData));

			auto address = constantBufferResource->GetGPUVirtualAddress();
			GetAppResources().commandList->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, skyboxHandle);
			GetAppResources().commandList->SetGraphicsRoot32BitConstants(EnvironmentRootIndices::EnvironmentRoughness, 1,&roughness, 0);
			GetAppResources().commandList->SetGraphicsRoot32BitConstants(EnvironmentRootIndices::EnvironmentFaceIndices, 1, &i, 0);
			GetAppResources().commandList->DrawInstanced(3, 1, 0, 0);
		}
	}

	transition = CD3DX12_RESOURCE_BARRIER::Transition(prefilteredMapTextures.resource.Get(), prefilteredMapTextures.currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);
	prefilteredMapTextures.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}



DescriptorHeapWrapper Environment::GetSRVDescriptorHeap()
{
	return srvDescriptorHeap;
}
