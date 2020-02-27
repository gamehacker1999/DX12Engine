#include "Game.h"
#include "Vertex.h"

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{

	prevMousePos = { 0,0 };

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif
	constantBufferBegin = nullptr;
	cameraBufferBegin = 0;
	lightCbufferBegin = 0;
	raster = true;

	memset(fenceValues, 0, sizeof(UINT64) * frameIndex);

}

Game::~Game()
{
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
	residencyManager.Destroy();
	
	//residencyManager.DestroyResidencySet(residencySet.get());
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
HRESULT Game::Init()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
		&options5, sizeof(options5)));
	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		throw std::runtime_error("Raytracing not supported on device");

	frameIndex = this->swapChain->GetCurrentBackBufferIndex();

	HRESULT hr;

	//residencyManager = std::make_shared<D3DX12Residency::ResidencyManager>();
	residencyManager.Initialize(device.Get(), 0, adapter.Get(), frameCount);
	residencySet = std::shared_ptr<D3DX12Residency::ResidencySet>(residencyManager.CreateResidencySet());

	residencySet->Open();

	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer));
	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		/*D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = frameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = (device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap)));
		if (FAILED(hr)) return hr;

		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);*/

		ThrowIfFailed(rtvDescriptorHeap.Create(device, frameCount, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

		//ThrowIfFailed(mainBufferHeap.Create(device, 4 + 6 + 1, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		//creating the depth stencil heap
		/*D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
		dsHeapDesc.NumDescriptors = 1;
		dsHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&dsHeapDesc, IID_PPV_ARGS(dsDescriptorHeap.GetAddressOf()));
		if (FAILED(hr)) return hr;*/

		ThrowIfFailed(dsDescriptorHeap.Create(device, 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

	}

	// Create frame resources.
	/**/{
		//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < frameCount; n++)
		{
			hr = (this->swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n].resource)));
			if (FAILED(hr)) return hr;
			device->CreateRenderTargetView(renderTargets[n].resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(n));
			//rtvHandle.Offset(1, rtvDescriptorSize);

			hr = (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
			if (FAILED(hr)) return hr;
		}

	}

	//creating depth stencil view
	/*D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
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
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1,1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(depthStencilBuffer.resource.GetAddressOf())
	));

	device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());*/

	dsDescriptorHeap.CreateDescriptor(depthStencilBuffer, RESOURCE_TYPE_DSV, device, 0, width, height);



	//create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), pipelineState.Get(),
		IID_PPV_ARGS(commandList.GetAddressOf())));

	//creating the skybox bundle
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(bundleAllocator.GetAddressOf())));

	//memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	//memcpy(constantBufferBegin+sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));

	
	//create synchronization object and wait till the objects have been passed to the gpu
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	fenceValues[frameIndex]++;
	//fence event handle for synchronization
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	LoadShaders();
	CreateMatrices();
	CreateBasicGeometry();
	CreateEnvironment();
	CreateAccelerationStructures();

	//allocate volumes and skyboxes here

	gpuHeapRingBuffer = std::make_shared<GPUHeapRingBuffer>(device);

	for (size_t i = 0; i < materials.size(); i++)
	{
		gpuHeapRingBuffer->AllocateStaticDescriptors(device, 4, materials[i]->GetDescriptorHeap());
		materials[i]->materialIndex = (UINT)i*4;
	}

	gpuHeapRingBuffer->AllocateStaticDescriptors(device, 1, skybox->GetDescriptorHeap());
	skybox->skyboxTextureIndex = gpuHeapRingBuffer->GetNumStaticResources()-1;

	flame = std::make_shared<RaymarchedVolume>(L"../../Assets/Textures/clouds.dds",mesh2,volumePSO,volumeRootSignature,device,commandQueue,mainBufferHeap,commandList);
	gpuHeapRingBuffer->AllocateStaticDescriptors(device, 1, flame->GetDescriptorHeap());
	flame->volumeTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;

	ThrowIfFailed(commandList->Close());

	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };
	//skybox->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition());
	skyboxBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	skyboxBundle->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	/**/skyboxBundle->SetPipelineState(skybox->GetPipelineState().Get());
	skyboxBundle->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
	skyboxBundle->SetGraphicsRootConstantBufferView(0, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
	skyboxBundle->SetGraphicsRootDescriptorTable(1, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->skyboxTextureIndex));
	skyboxBundle->IASetVertexBuffers(0, 1, &skybox->GetMesh()->GetVertexBuffer());
	skyboxBundle->IASetIndexBuffer(&skybox->GetMesh()->GetIndexBuffer());
	skyboxBundle->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	skyboxBundle->Close();

	mainCamera = std::make_shared<Camera>(XMFLOAT3(0.0f, 0.f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f));

	mainCamera->CreateProjectionMatrix((float)width / height); //creating the camera projection matrix


	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	residencySet->Close();

	WaitForPreviousFrame();
	CreateRayTracingPipeline();
	CreateRaytracingOutputBuffer();
	CreateRaytracingDescriptorHeap();
	CreateShaderBindingTable();

	return S_OK;
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files 
// --------------------------------------------------------
void Game::LoadShaders()
{

	//this describes the type of constant buffer and which register to map the data to
	CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
	CD3DX12_ROOT_PARAMETER1 rootParams[5]; // specifies the descriptor table
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[1].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[2].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[4].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplers[0].Init(0);

	//rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);


	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
		signature.GetAddressOf(), error.GetAddressOf()));
	//if (FAILED(hr)) return hr;

	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf())));

	//if (FAILED(hr)) return hr;
	ComPtr<ID3DBlob> vertexShaderBlob;
	ComPtr<ID3DBlob> pixelShaderBlob;
	ComPtr<ID3DBlob> pbrPixelShaderBlob;
	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShaderPBR.cso", pbrPixelShaderBlob.GetAddressOf()));

	//input vertex layout, describes the semantics

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

	};

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	////psoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescPBR = {};
	psoDescPBR.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	psoDescPBR.pRootSignature = rootSignature.Get();
	psoDescPBR.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	psoDescPBR.PS = CD3DX12_SHADER_BYTECODE(pbrPixelShaderBlob.Get());
	////psoPBRDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//psoDePBRsc.DepthStencilState.StencilEnable = FALSE;
	psoDescPBR.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDescPBR.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDescPBR.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	psoDescPBR.SampleMask = UINT_MAX;
	psoDescPBR.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDescPBR.NumRenderTargets = 1;
	psoDescPBR.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDescPBR.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDescPBR.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescPBR, IID_PPV_ARGS(pbrPipelineState.GetAddressOf())));

	CD3DX12_DESCRIPTOR_RANGE1 volumeRanges[1];
	CD3DX12_ROOT_PARAMETER1 volumeRootParams[2];

	volumeRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	volumeRootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL);
	volumeRootParams[1].InitAsDescriptorTable(1, &volumeRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);

	ComPtr<ID3DBlob> volumeSignature;
	ComPtr<ID3DBlob> volumeError;

	CD3DX12_STATIC_SAMPLER_DESC staticSamplersVolume[1];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplersVolume[0].Init(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	rootSignatureDesc.Init_1_1(_countof(volumeRootParams), volumeRootParams, _countof(staticSamplersVolume), staticSamplersVolume, rootSignatureFlags);

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,D3D_ROOT_SIGNATURE_VERSION_1_1,volumeSignature.GetAddressOf(),volumeError.GetAddressOf()));

	ThrowIfFailed(device->CreateRootSignature(0, volumeSignature->GetBufferPointer(), volumeSignature->GetBufferSize(), IID_PPV_ARGS(volumeRootSignature.GetAddressOf())));
	
	ComPtr<ID3DBlob> rayMarchedVolumeVS;
	ComPtr<ID3DBlob> raymarcedVolumePS;

	ThrowIfFailed(D3DReadFileToBlob(L"VolumeRayMarcherPS.cso", raymarcedVolumePS.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"VolumeRayMarcherVS.cso", rayMarchedVolumeVS.GetAddressOf()));

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescVolume = {};
	psoDescVolume.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	psoDescVolume.pRootSignature = volumeRootSignature.Get();
	psoDescVolume.VS = CD3DX12_SHADER_BYTECODE(rayMarchedVolumeVS.Get());
	psoDescVolume.PS = CD3DX12_SHADER_BYTECODE(raymarcedVolumePS.Get());
	////psoPBRDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//psoDePBRsc.DepthStencilState.StencilEnable = FALSE;
	psoDescVolume.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDescVolume.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDescVolume.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state
	psoDescVolume.BlendState.AlphaToCoverageEnable = false;
	psoDescVolume.BlendState.IndependentBlendEnable = false;
	psoDescVolume.BlendState.RenderTarget[0].BlendEnable = true;
	psoDescVolume.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDescVolume.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_COLOR;
	//psoDescVolume.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	psoDescVolume.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDescVolume.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDescVolume.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	psoDescVolume.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDescVolume.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDescVolume.SampleMask = UINT_MAX;
	psoDescVolume.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDescVolume.NumRenderTargets = 1;
	psoDescVolume.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDescVolume.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDescVolume.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescVolume, IID_PPV_ARGS(volumePSO.GetAddressOf())));

	//creating particle root sig and pso
	CD3DX12_DESCRIPTOR_RANGE1 particleDescriptorRange[2];
	CD3DX12_ROOT_PARAMETER1 particleRootParams[3];

	particleDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
	particleRootParams[1].InitAsDescriptorTable(1, &particleDescriptorRange[1], D3D12_SHADER_VISIBILITY_PIXEL);
	particleRootParams[2].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

	ComPtr<ID3DBlob> particleSignature;
	ComPtr<ID3DBlob> particleError;

	CD3DX12_STATIC_SAMPLER_DESC staticSamplersParticle[1];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplersParticle[0].Init(0);

	rootSignatureDesc.Init_1_1(_countof(particleRootParams), particleRootParams, 
		_countof(staticSamplersParticle), staticSamplersParticle, rootSignatureFlags);

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, 
		particleSignature.GetAddressOf(), particleError.GetAddressOf()));

	ThrowIfFailed(device->CreateRootSignature(0, particleSignature->GetBufferPointer(), particleSignature->GetBufferSize(), 
		IID_PPV_ARGS(particleRootSig.GetAddressOf())));

	ComPtr<ID3DBlob> particleVS;
	ComPtr<ID3DBlob> particlePS;

	ThrowIfFailed(D3DReadFileToBlob(L"ParticlePS.cso", particlePS.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"ParticleVS.cso", particleVS.GetAddressOf()));

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescParticle = {};
	//psoDescParticle.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	psoDescParticle.pRootSignature = particleRootSig.Get();
	psoDescParticle.VS = CD3DX12_SHADER_BYTECODE(particleVS.Get());
	psoDescParticle.PS = CD3DX12_SHADER_BYTECODE(particlePS.Get());
	//psoDescParticle.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDescParticle.DepthStencilState.DepthEnable = true;
	psoDescParticle.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDescParticle.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDescParticle.DepthStencilState.DepthEnable = true;
	psoDescParticle.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDescParticle.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state
	psoDescParticle.BlendState.AlphaToCoverageEnable = false;
	psoDescParticle.BlendState.IndependentBlendEnable = false;
	psoDescParticle.BlendState.RenderTarget[0].BlendEnable = true;
	psoDescParticle.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDescParticle.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDescParticle.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	psoDescParticle.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDescParticle.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	psoDescParticle.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDescParticle.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDescParticle.SampleMask = UINT_MAX;
	psoDescParticle.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDescParticle.NumRenderTargets = 1;
	psoDescParticle.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDescParticle.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDescParticle.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescParticle, IID_PPV_ARGS(particlesPSO.GetAddressOf())));
}



// --------------------------------------------------------
// Initializes the matrices necessary to represent our geometry's 
// transformations and our 3D camera
// --------------------------------------------------------
void Game::CreateMatrices()
{
	// Set up world matrix
	XMMATRIX W = XMMatrixIdentity();
	XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(W)); // Transpose for HLSL!

	// Create the View matrix
	XMVECTOR pos = XMVectorSet(0, 0, -5, 0);
	XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	XMMATRIX V = XMMatrixLookToLH(
		pos,     // The position of the "camera"
		dir,     // Direction the camera is looking
		up);     // "Up" direction in 3D space (prevents roll)
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(V)); // Transpose for HLSL!

	// Create the Projection matrix
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * 3.1415926535f,		// Field of View Angle
		(float)width / height,		// Aspect ratio
		0.1f,						// Near clip plane distance
		100.0f);					// Far clip plane distance
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{

	//creatng the constant buffer heap before creating the entity
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer)*3),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(cbufferUploadHeap.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024*64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightConstantBufferResource.GetAddressOf())
	));

	ZeroMemory(&lightData, sizeof(lightData));

	lightData.light1.diffuse = XMFLOAT4(1, 0, 0, 1);
	lightData.light1.direction = XMFLOAT3(1, 0, 0);
	lightData.light1.ambientColor = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
	lightData.light1.specularity = XMFLOAT4(1, 0, 0, 1);

	lightConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightCbufferBegin));
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	UINT64 cbufferOffset = 0;
	mesh1 = std::make_shared<Mesh>("../../Assets/Models/sphere.obj", device, commandList);
	mesh2 = std::make_shared<Mesh>("../../Assets/Models/cube.obj", device, commandList);
	mesh3 = std::make_shared<Mesh>("../../Assets/Models/helix.obj", device, commandList);
	

	//creating the vertex buffer
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	float aspectRatio = static_cast<float>(width / height);

	//CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mainCPUDescriptorHandle, 0, cbvDescriptorSize);
	material1 = std::make_shared<Material>(device, commandQueue,mainBufferHeap, pbrPipelineState,rootSignature,
		L"../../Assets/Textures/LayeredDiffuse.png", L"../../Assets/Textures/LayeredNormal.png",
		L"../../Assets/Textures/LayeredRoughness.png",L"../../Assets/Textures/LayeredMetallic.png");
	material2 = std::make_shared<Material>(device, commandQueue, mainBufferHeap, pipelineState, rootSignature,
		L"../../Assets/Textures/RocksDiffuse.jpg",L"../../Assets/Textures/RocksNormal.jpg");

	materials.emplace_back(material1);
	materials.emplace_back(material2);


	entity1 = std::make_shared<Entity>(mesh2,material1);
	entity2 = std::make_shared<Entity>(mesh1,material1);
	entity3 = std::make_shared<Entity>(mesh1,material2);
	entity4 = std::make_shared<Entity>(mesh3,material1);
	

	entity1->SetPosition(XMFLOAT3(0, -10, 1.5f));
	entity1->SetScale(XMFLOAT3(10, 10, 10));
	entity2->SetPosition(XMFLOAT3(1, 0, 1.0f));
	entity3->SetPosition(XMFLOAT3(-1, 0, 1.f));
	entity4->SetPosition(XMFLOAT3(-4, 0, 1.f));


	entity1->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity2->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity3->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity4->PrepareConstantBuffers(device,residencyManager,residencySet);


	entities.emplace_back(entity1);
	entities.emplace_back(entity2);
	entities.emplace_back(entity3);
	entities.emplace_back(entity4);
	entities.emplace_back(std::make_shared<Entity>(mesh3, material2));

	entities[entities.size() - 1]->SetPosition(XMFLOAT3(0, 2, 0));
	entities[entities.size() - 1]->PrepareConstantBuffers(device,residencyManager,residencySet);


	//copying the data from upload heaps to default heaps


	//creating the constant buffer
	/*ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),//must be a multiple of 64kb
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constantBufferResource.GetAddressOf())
	));*/

	//create a constant buffer view
	/*D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress(); //gpu address of the constant buffer
	cbvDesc.SizeInBytes = (sizeof(SceneConstantBuffer) + 255) & ~255;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(constantBufferHeap->GetCPUDescriptorHandleForHeapStart(), 0, 0);
	device->CreateConstantBufferView(&cbvDesc, cbvHandle);
	cbvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc2 = {};
	cbvDesc2.BufferLocation = constantBuffer->GetGPUVirtualAddress() + (sizeof(SceneConstantBuffer) + 255) & ~255;
	cbvDesc2.SizeInBytes = (sizeof(SceneConstantBuffer) + 255) & ~255;
	device->CreateConstantBufferView(&cbvDesc2, cbvHandle);*/

	/*ZeroMemory(&constantBufferData, sizeof(constantBufferData));

	//setting range to 0,0 so that the cpu cannot read from this resource
	//can keep the constant buffer mapped for the entire application
	ThrowIfFailed(constantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&constantBufferBegin)));
	for (int i = 0; i < entities.size(); i++)
	{
		memcpy(constantBufferBegin + (i * sceneConstantBufferAlignmentSize), &constantBufferData, sizeof(constantBufferData));
	}*/




}

void Game::CreateEnvironment()
{
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	CD3DX12_ROOT_PARAMETER1 rootParams[2]; // specifies the descriptor table
	rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplers[0].Init(0);

	//rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
		signature.GetAddressOf(), error.GetAddressOf()));
	//if (FAILED(hr)) return hr;

	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(skyboxRootSignature.GetAddressOf())));

	//if (FAILED(hr)) return hr;
	ComPtr<ID3DBlob> vertexShaderBlob;
	ComPtr<ID3DBlob> pixelShaderBlob;
	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"CubeMapVS.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"CubeMapPS.cso", pixelShaderBlob.GetAddressOf()));

	//input vertex layout, describes the semantics

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

	};

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	psoDesc.pRootSignature = skyboxRootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	////psoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(skyboxPSO.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mainCPUDescriptorHandle, (INT)entities.size()+1, cbvDescriptorSize);
	//creating the skybox
	skybox = std::make_shared<Skybox>(L"../../Assets/Textures/skybox3.dds", mesh2, skyboxPSO, skyboxRootSignature, device, commandQueue, mainBufferHeap);

	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleAllocator.Get(), skyboxPSO.Get(), IID_PPV_ARGS(skyboxBundle.GetAddressOf())));


	/*ID3D12DescriptorHeap* ppHeaps[] = { mainBufferHeap.GetHeap().Get() };
	skyboxBundle->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	skyboxBundle->SetPipelineState(skybox->GetPipelineState().Get());
	skyboxBundle->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
	skyboxBundle->SetGraphicsRootConstantBufferView(0, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
	skyboxBundle->SetGraphicsRootDescriptorTable(1, mainBufferHeap.GetGPUHandle(skybox->GetSkyboxTexture().heapOffset));
	skyboxBundle->IASetVertexBuffers(0, 1, &skybox->GetMesh()->GetVertexBuffer());
	skyboxBundle->IASetIndexBuffer(&skybox->GetMesh()->GetIndexBuffer());
	skyboxBundle->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	ThrowIfFailed(skyboxBundle->Close());*/

}

void Game::CreateShaderBindingTable()
{
	//shader binding tables define the raygen, miss, and hit group shaders
	//these resources are interpreted by the shader
	sbtGenerator.Reset();

	//getting the descriptor heap handle
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = rtDescriptorHeap.GetHeap()->GetGPUDescriptorHandleForHeapStart();

	//reinterpreting the above pointer as a void pointer
	auto heapPointer = reinterpret_cast<UINT64*>(gpuHandle.ptr);

	//the ray generation shader needs external data therefore it needs the pointer to the heap
	//the miss and hit group shaders don't have any data
	sbtGenerator.AddRayGenerationProgram(L"RayGen", { heapPointer });
	sbtGenerator.AddMissProgram(L"Miss", {heapPointer});
	for (int i = 0; i < 2; i++)
	{
		sbtGenerator.AddHitGroup(L"HitGroup", { (void*)entities[3]->GetMesh()->GetVertexBufferResourceAndCount().first.Get()->GetGPUVirtualAddress(),(void*)lightConstantBufferResource->GetGPUVirtualAddress() });
		sbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
	}
	sbtGenerator.AddHitGroup(L"PlaneHitGroup", { (void*)entities[0]->GetMesh()->GetVertexBufferResourceAndCount().first.Get()->GetGPUVirtualAddress(),heapPointer});

	sbtGenerator.AddMissProgram(L"ShadowMiss", {});
	sbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

	//compute the size of the SBT
	UINT32 sbtSize = sbtGenerator.ComputeSBTSize();

	//upload heap for the sbt
	sbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	if (!sbtResource)
		throw std::logic_error("Could not create SBT resource");

	//compile the sbt from the above info
	sbtGenerator.Generate(sbtResource.Get(), rtStateObjectProps.Get());
}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update our projection matrix since the window size changed
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * 3.1415926535f,	// Field of View Angle
		(float)width / height,	// Aspect ratio
		0.1f,				  	// Near clip plane distance
		100.0f);			  	// Far clip plane distance
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!
}

AccelerationStructureBuffers Game::CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vertexBuffers)
{
	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS; //bottom level as generator

	for (const auto& buffer : vertexBuffers)
	{
		bottomLevelAS.AddVertexBuffer(buffer.first.Get(), 0, buffer.second, sizeof(Vertex), 0, 0);
	}

	//allocating scratch space to store temporary information
	UINT64 scratchInBytes = 0;
	UINT64 resultSizeInBytes = 0;

	bottomLevelAS.ComputeASBufferSizes(device.Get(), false, &scratchInBytes, &resultSizeInBytes);

	//onmce the size is obtained ,then we need to create the necessary buffers
	AccelerationStructureBuffers buffers;
	buffers.pScratch = nv_helpers_dx12::CreateBuffer(device.Get(), scratchInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
	buffers.pResult = nv_helpers_dx12::CreateBuffer(device.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);
	
	//build the acceleration structure
	bottomLevelAS.Generate(commandList.Get(), buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);

	return buffers;
}

void Game::CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, XMMATRIX>>& instances)
{
	for (int i = 0; i < instances.size()-1; i++)
	{
		topLevelAsGenerator.AddInstance(instances[i].first.Get(), instances[i].second, static_cast<UINT>(i), static_cast<UINT>(i*2),D3D12_RAYTRACING_INSTANCE_FLAG_NONE,0xFF);
	}

	topLevelAsGenerator.AddInstance(instances[2].first.Get(), instances[2].second, static_cast<UINT>(2), static_cast<UINT>(2 * 2), D3D12_RAYTRACING_INSTANCE_FLAG_NONE, 0x02);


	UINT64 scratchSize, resultSize, instanceDescsSize;

	//allocating scratch space, result space, and instance space
	topLevelAsGenerator.ComputeASBufferSizes(device.Get(), true, &scratchSize, &resultSize, &instanceDescsSize);

	topLevelAsBuffers.pScratch = nv_helpers_dx12::CreateBuffer(device.Get(), 
		scratchSize, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		nv_helpers_dx12::kDefaultHeapProps);

	topLevelAsBuffers.pResult = nv_helpers_dx12::CreateBuffer(device.Get(), resultSize, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
		nv_helpers_dx12::kDefaultHeapProps);

	// The buffer describing the instances: ID, shader binding information,
	// matrices ... Those will be copied into the buffer by the helper through
	// mapping, so the buffer has to be allocated on the upload heap.
	topLevelAsBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
		device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	topLevelAsGenerator.Generate(commandList.Get(),
		topLevelAsBuffers.pScratch.Get(),
		topLevelAsBuffers.pResult.Get(),
		topLevelAsBuffers.pInstanceDesc.Get());

}

void Game::CreateAccelerationStructures()
{
	//creating a bottom level AS for the first entity for now
	AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS({ entities[3]->GetMesh()->GetVertexBufferResourceAndCount() });
	AccelerationStructureBuffers planeBottomLevelBuffer = CreateBottomLevelAS({ entities[0]->GetMesh()->GetVertexBufferResourceAndCount() });

	//create only one instance for now

	//instances.emplace_back(std::pair<ComPtr<ID3D12Resource>, XMFLOAT4X4>(bottomLevelBuffers.pResult, entities[3]->GetModelMatrix()));

	instances = { {bottomLevelBuffers.pResult, entities[3]->GetRawModelMatrix()},{bottomLevelBuffers.pResult, XMMatrixTranslation(-1,3,20)},{planeBottomLevelBuffer.pResult,entities[0]->GetRawModelMatrix()} };
	CreateTopLevelAS(instances);

	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForPreviousFrame();

	ThrowIfFailed(
		commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

	bottomLevelAs = bottomLevelBuffers.pResult;

}

ComPtr<ID3D12RootSignature> Game::CreateRayGenRootSignature()
{
	//this ray generation shader is getting and output texture and an acceleration structure
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({ {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,0}, //ouput texture
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1},{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,2}
	});

	//rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0, 0);

	return rsc.Generate(device.Get(), true);
}

ComPtr<ID3D12RootSignature> Game::CreateMissRootSignature()
{
	//the miss signature only has a payload
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({ { 0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3 } });
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc;
	samplerDesc.Init(0);
	rsc.AddStaticSamplers(samplerDesc);
	return rsc.Generate(device.Get(), true);
}

ComPtr<ID3D12RootSignature> Game::CreateClosestHitRootSignature()
{
	//the hit signature only has a payload
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV);
	rsc.AddHeapRangesParameter({ {1,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1} });
	return rsc.Generate(device.Get(), true);

}

void Game::CreateRaytracingOutputBuffer()
{
	//create the output texture that will be used to store texture data
	D3D12_RESOURCE_DESC resDes = {};
	resDes.DepthOrArraySize = 1;
	resDes.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDes.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDes.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDes.Width = width;
	resDes.Height = height;
	resDes.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDes.MipLevels = 1;
	resDes.SampleDesc.Count = 1;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(rtOutPut.resource.GetAddressOf())));
	rtOutPut.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtOutPut.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes,D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtNormals.resource.GetAddressOf())));
	rtNormals.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtNormals.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtDiffuse.resource.GetAddressOf())));
	rtDiffuse.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtDiffuse.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtPosition.resource.GetAddressOf())));
	rtPosition.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtPosition.resourceType = RESOURCE_TYPE_UAV;
}

void Game::CreateRaytracingDescriptorHeap()
{
	//creating the descriptor heap, it will contain two descriptors
	//one for the UAV output and an SRV for the acceleration structure
	rtDescriptorHeap.Create(device, 4, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	rtDescriptorHeap.CreateDescriptor(rtOutPut, rtOutPut.resourceType, device);
	//initializing the camera buffer used for raytracing
	//ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		//D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(cameraData.resource.GetAddressOf())));

	rtDescriptorHeap.CreateRaytracingAccelerationStructureDescriptor(device, rtOutPut, topLevelAsBuffers);
	rtDescriptorHeap.CreateDescriptor(cameraData, RESOURCE_TYPE_CBV, device, sizeof(RayTraceCameraData));
	rtDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/skybox3.dds",skyboxTexResource,RESOURCE_TYPE_SRV,device,commandQueue,TEXTURE_TYPE_DDS);
	ZeroMemory(&rtCamera, sizeof(rtCamera));
	ThrowIfFailed(cameraData.resource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&cameraBufferBegin)));
	memcpy(cameraBufferBegin, &rtCamera, sizeof(rtCamera));

}

void Game::CreateRayTracingPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	rayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../RayGen.hlsl");
	missLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Miss.hlsl");
	hitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Hit.hlsl");


	//add the libraies to pipeliene
	pipeline.AddLibrary(rayGenLib.Get(), { L"RayGen" });
	pipeline.AddLibrary(missLib.Get(), { L"Miss" });
	pipeline.AddLibrary(hitLib.Get(), { L"ClosestHit",L"PlaneClosestHit"});

	//creating the root signatures
	rayGenRootSig = CreateRayGenRootSignature();
	missRootSig = CreateMissRootSignature();
	closestHitRootSignature = CreateClosestHitRootSignature();

	shadowRayLib = nv_helpers_dx12::CompileShaderLibrary(L"../../ShadowRay.hlsl");
	pipeline.AddLibrary(shadowRayLib.Get(), { L"ShadowClosestHit",L"ShadowMiss" });
	shadowRootSig = CreateClosestHitRootSignature();

	//adding a hit group to the pipeline
	pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
	pipeline.AddHitGroup(L"PlaneHitGroup", L"PlaneClosestHit");
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");

	//associating the root signatures with the shaders
	//shaders can share root signatures
	pipeline.AddRootSignatureAssociation(rayGenRootSig.Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(missRootSig.Get(), { L"Miss",L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"HitGroup" ,L"PlaneHitGroup",L"ShadowHitGroup"}); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
	//pipeline.AddRootSignatureAssociation(shadowRootSig.Get(), { L"ShadowHitGroup" });

	//payload size defines the maximum size of the data carried by the rays
	pipeline.SetMaxPayloadSize(4 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(2);

	//creating the state obj
	rtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(rtStateObject->QueryInterface(IID_PPV_ARGS(rtStateObjectProps.GetAddressOf())));
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();

	mainCamera->Update(deltaTime);

	if (GetAsyncKeyState('R')) raster = !raster;

	lightData.cameraPosition = mainCamera->GetPosition();
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	rtCamera.view = mainCamera->GetViewMatrix();
	rtCamera.proj = mainCamera->GetProjectionMatrix();
	XMMATRIX viewTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.view));
	XMStoreFloat4x4(&rtCamera.iView, XMMatrixTranspose(XMMatrixInverse(nullptr,viewTranspose)));
	XMMATRIX projTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.proj));
	XMStoreFloat4x4(&rtCamera.iProj, XMMatrixTranspose(XMMatrixInverse(nullptr, projTranspose)));
	//
	memcpy(cameraBufferBegin, &rtCamera, sizeof(rtCamera));

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	PopulateCommandList();

	//execute the commanf list
	ID3D12CommandList* pcommandLists[] = { commandList.Get() };
	D3DX12Residency::ResidencySet* ppSets[] = { residencySet.get() };
	commandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);
	//residencyManager.ExecuteCommandLists(commandQueue.Get(), pcommandLists, ppSets, 1);
	//present the frame
	ThrowIfFailed(swapChain->Present(1, 0));

	MoveToNextFrame();
}

void Game::PopulateCommandList()
{

		ThrowIfFailed(commandAllocators[frameIndex]->Reset());
		ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

		residencySet->Open();

		//set necessary state
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);

		//setting the constant buffer descriptor table
		ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		//indicate that the back buffer is the render target
		commandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].resource.Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap.GetCPUHandle(frameIndex);//(rtvDescriptorHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart(),
			//frameIndex,rtvDescriptorSize);
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset));
		commandList->ClearDepthStencilView(dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		/**/

	if (raster)
	{
			//record commands
		const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetBeginningStaticResourceOffset();//(mainBufferHeap->GetGPUDescriptorHandleForHeapStart(),0,cbvDescriptorSize);
		commandList->SetGraphicsRootDescriptorTable(3, gpuCBVSRVUAVHandle);
		//gpuCBVSRVUAVHandle.Offset(gpuHeapRingBuffer->GetBeginningStaticResourceOffset());
		gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetStaticDescriptorOffset();
		//commandList->SetGraphicsRootDescriptorTable(0, gpuHeapRingBuffer->GetStaticDescriptorOffset());

		/**/for (UINT i = 0; i < entities.size(); i++)
		{
			entities[i]->PrepareMaterial(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());
			gpuHeapRingBuffer->AddDescriptor(device, 1, entities[i]->GetDescriptorHeap(), 0);
			commandList->SetGraphicsRootDescriptorTable(0, gpuHeapRingBuffer->GetDynamicResourceOffset());
			commandList->SetGraphicsRoot32BitConstant(1, i, 0);
			commandList->SetGraphicsRootSignature(entities[i]->GetRootSignature().Get());
			commandList->SetPipelineState(entities[i]->GetPipelineState().Get());
			commandList->SetGraphicsRoot32BitConstant(4, entities[i]->GetMaterialIndex(), 0);
			D3D12_VERTEX_BUFFER_VIEW vertexBuffer = entities[i]->GetMesh()->GetVertexBuffer();
			auto indexBuffer = entities[i]->GetMesh()->GetIndexBuffer();
			commandList->SetGraphicsRootConstantBufferView(2, lightConstantBufferResource->GetGPUVirtualAddress());

			commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
			commandList->IASetIndexBuffer(&indexBuffer);
			commandList->DrawIndexedInstanced(entities[i]->GetMesh()->GetIndexCount(), 1, 0, 0, 0);

		}

		//drawing the skybox
		//gpuCBVSRVUAVHandle.Offset((INT)entities.size() * cbvDescriptorSize);

		skybox->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition());
		/*commandList->SetPipelineState(skybox->GetPipelineState().Get());
		commandList->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
		commandList->SetGraphicsRootConstantBufferView(0, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(1, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->skyboxTextureIndex));
		commandList->IASetVertexBuffers(0, 1, &skybox->GetMesh()->GetVertexBuffer());
		commandList->IASetIndexBuffer(&skybox->GetMesh()->GetIndexBuffer());
		commandList->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);*/

		commandList->ExecuteBundle(skyboxBundle.Get());

		flame->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition(), totalTime);
		commandList->SetPipelineState(flame->GetPipelineState().Get());
		commandList->SetGraphicsRootSignature(flame->GetRootSignature().Get());
		commandList->SetGraphicsRootConstantBufferView(0, flame->GetConstantBuffer()->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(1, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(flame->volumeTextureIndex));
		commandList->IASetVertexBuffers(0, 1, &flame->GetMesh()->GetVertexBuffer());
		commandList->IASetIndexBuffer(&flame->GetMesh()->GetIndexBuffer());
		commandList->DrawIndexedInstanced(flame->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
		///back buffer will now be used to present
	}

	else
	{
		ID3D12DescriptorHeap* ppHeaps[] = { rtDescriptorHeap.GetHeap().Get() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
			rtOutPut.resource.Get(), rtOutPut.currentState,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		rtOutPut.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(1, &transition);

		const float clearColor[] = { 0.6f, 0.8f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		//creating a dispatch rays description
		D3D12_DISPATCH_RAYS_DESC desc = {};
		//raygeneration location
		desc.RayGenerationShaderRecord.StartAddress = sbtResource->GetGPUVirtualAddress();
		//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
		desc.RayGenerationShaderRecord.SizeInBytes = sbtGenerator.GetRayGenSectionSize();

		//miss shaders
		desc.MissShaderTable.StartAddress = sbtResource->GetGPUVirtualAddress() + sbtGenerator.GetRayGenSectionSize();
		desc.MissShaderTable.SizeInBytes = sbtGenerator.GetMissSectionSize();
		//desc.MissShaderTable.StartAddress = (desc.MissShaderTable.StartAddress + 63) & ~63;
		desc.MissShaderTable.StrideInBytes = sbtGenerator.GetMissEntrySize();

		//hit groups
		desc.HitGroupTable.StartAddress = sbtResource->GetGPUVirtualAddress() + sbtGenerator.GetRayGenSectionSize() + sbtGenerator.GetMissSectionSize();
		//desc.HitGroupTable.StartAddress = (desc.HitGroupTable.StartAddress + 63) & ~63;
		desc.HitGroupTable.SizeInBytes = sbtGenerator.GetHitGroupSectionSize();
		desc.HitGroupTable.StrideInBytes = sbtGenerator.GetHitGroupEntrySize();

		//scene description
		desc.Height = height;
		desc.Width = width;
		desc.Depth = 1;

		commandList->SetPipelineState1(rtStateObject.Get());
		commandList->DispatchRays(&desc);

		// The raytracing output needs to be copied to the actual render target used
		// for display. For this, we need to transition the raytracing output from a
		// UAV to a copy source, and the render target buffer to a copy destination.
		// We can then do the actual copy, before transitioning the render target
		// buffer into a render target, that will be then used to display the image
		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			rtOutPut.resource.Get(), rtOutPut.currentState,
			D3D12_RESOURCE_STATE_COPY_SOURCE);

		rtOutPut.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

		commandList->ResourceBarrier(1, &transition);

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[frameIndex].resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_DEST);

		commandList->ResourceBarrier(1, &transition);

		commandList->CopyResource(renderTargets[frameIndex].resource.Get(),
			rtOutPut.resource.Get());

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[frameIndex].resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &transition);

	}
		// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	commandList->Close();
	residencySet->Close();

}

void Game::WaitForPreviousFrame()
{
	//signal and increment the fence
	//const UINT64 pfence = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(),fenceValues[frameIndex]));
	//fenceValue++;

	//wait until the previous frame is finished
	//if (fence->GetCompletedValue() < pfence)
	//{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE,false);
	//}
	
	//WaitToFlushGPU(commandQueue,fence,fenceValue,fenceEvent);
	//frameIndex = swapChain->GetCurrentBackBufferIndex();
	fenceValues[frameIndex]++;
}

void Game::MoveToNextFrame()
{
	const UINT64 currentFenceValues = fenceValues[frameIndex];
	ThrowIfFailed(commandQueue->Signal(fence.Get(), currentFenceValues));

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	//if the frame is not ready to be rendered yet then wait for it to be ready
	if (fence->GetCompletedValue() < fenceValues[frameIndex])
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, false);
	}

	//set the fence value of the next frame
	fenceValues[frameIndex] = currentFenceValues + 1;
}


#pragma region Mouse Input

// --------------------------------------------------------
// Helper method for mouse clicking.  We get this information
// from the OS-level messages anyway, so these helpers have
// been created to provide basic mouse input if you want it.
// --------------------------------------------------------
void Game::OnMouseDown(WPARAM buttonState, int x, int y)
{
	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	// Caputure the mouse so we keep getting mouse move
	// events even if the mouse leaves the window.  we'll be
	// releasing the capture once a mouse button is released
	SetCapture(hWnd);
}

// --------------------------------------------------------
// Helper method for mouse release
// --------------------------------------------------------
void Game::OnMouseUp(WPARAM buttonState, int x, int y)
{
	// We don't care about the tracking the cursor outside
	// the window anymore (we're not dragging if the mouse is up)
	ReleaseCapture();
}

// --------------------------------------------------------
// Helper method for mouse movement.  We only get this message
// if the mouse is currently over the window, or if we're 
// currently capturing the mouse.
// --------------------------------------------------------
void Game::OnMouseMove(WPARAM buttonState, int x, int y)
{
	// Save the previous mouse position, so we have it for the future

	if (buttonState & 0x0001)
	{
		int deltaX = x - prevMousePos.x;
		int deltaY = y - prevMousePos.y;

		//changing the yaw and pitch of the camera
		mainCamera->ChangeYawAndPitch((float)deltaX, (float)deltaY);
	}
	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;
}

// --------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending 
// on the direction of the scroll
// --------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
}
#pragma endregion