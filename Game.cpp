#include "Game.h"
#include"LTC.h"
#include "Vertex.h"
#include"FlockingSystem.h"

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
	lightingCbufferBegin = 0;
	lightCount = 0;
	raster = true;

	memset(fenceValues, 0, sizeof(UINT64) * frameIndex);
	memset(&lightingData, 0, sizeof(LightingData));

	isRaytracingAllowed = false;
	rtToggle = true;
	enableSSS = false;

}

Game::~Game()
{
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
	residencyManager.Destroy();

	//for (int i = 0; i < flockers.size(); i++)
	//{
	//	registry.destroy(flockers[i]->GetEntityID());
	//}
	
	//delete[] denoised_pixels;
	//inputBuffer->destroy();
	//normalBuffer->destroy();
	//albedoBuffer->destroy();
	//outBuffer->destroy();
	//optixContext->destroy();
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
		isRaytracingAllowed = false;
	else
		isRaytracingAllowed = true;

	frameIndex = this->swapChain->GetCurrentBackBufferIndex();

	HRESULT hr;

	InitOptix();


	//residencyManager = std::make_shared<D3DX12Residency::ResidencyManager>();
	residencyManager.Initialize(device.Get(), 0, adapter.Get(), frameCount);
	residencySet = std::shared_ptr<D3DX12Residency::ResidencySet>(residencyManager.CreateResidencySet());

	residencySet->Open();

	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer));
	// Create descriptor heaps.
	{

		ThrowIfFailed(rtvDescriptorHeap.Create(device, frameCount, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

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

	if(isRaytracingAllowed)
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
	skybox->CreateEnvironment(commandList, device, skyboxRootSignature, skyboxRootSignature, irradiencePSO, prefilteredMapPSO, brdfLUTPSO, dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset));
	gpuHeapRingBuffer->AllocateStaticDescriptors(device, 3, skybox->GetEnvironmentHeap());
	skybox->environmentTexturesIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1-2;

	CreateLTCTexture();

	flame = std::make_shared<RaymarchedVolume>(L"../../Assets/Textures/clouds.dds",mesh2,volumePSO,
		volumeRootSignature,device,commandQueue,mainBufferHeap,commandList);
	gpuHeapRingBuffer->AllocateStaticDescriptors(device, 1, flame->GetDescriptorHeap());
	flame->volumeTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;

	emitter1 = std::make_shared<Emitter>(1000, //max particles
		50, //particles per second
		2.f, //lifetime
		0.8f, //start size
		0.03f, //end size
		XMFLOAT4(1, 1.0f, 1.0f, 1.0f), //start color
		XMFLOAT4(1, 0.1f, 0.1f, 0.6f), //end color
		XMFLOAT3(0, 1, 0.f), //start vel
		XMFLOAT3(0.2f, 0.2f, 0.2f), //velocity deviation range
		XMFLOAT3(5, 0, 0), //start position
		XMFLOAT3(0.1f, 0.1f, 0.1f), //position deviation range
		XMFLOAT4(-2, 2, -2, 2), //rotation around z axis
		XMFLOAT3(0.f, 1.f, 0.f),  //acceleration	
		device,commandList,commandQueue,particlesPSO,particleRootSig,L"../../Assets/Textures/particle.jpg");

	gpuHeapRingBuffer->AllocateStaticDescriptors(device, 1, emitter1->GetDescriptor());
	emitter1->particleTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;

	ThrowIfFailed(commandList->Close());

	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };
	//skybox->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition());
	skyboxBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	skyboxBundle->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	/**/skyboxBundle->SetPipelineState(skybox->GetPipelineState().Get());
	skyboxBundle->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
	skyboxBundle->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentVertexCBV, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
	skyboxBundle->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->skyboxTextureIndex));
	skyboxBundle->IASetVertexBuffers(0, 1, &skybox->GetMesh()->GetVertexBuffer());
	skyboxBundle->IASetIndexBuffer(&skybox->GetMesh()->GetIndexBuffer());
	skyboxBundle->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	skyboxBundle->Close();

	mainCamera = std::make_shared<Camera>(XMFLOAT3(3.0f, 0.f, -3.0f), XMFLOAT3(-1.0f, 0.0f, 1.0f));

	mainCamera->CreateProjectionMatrix((float)width / height); //creating the camera projection matrix


	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	residencySet->Close();

	WaitForPreviousFrame();

	if (isRaytracingAllowed)
	{
		CreateGbufferRaytracingPipeline();
		CreateRayTracingPipeline();
		CreateRaytracingOutputBuffer();
		CreateRaytracingDescriptorHeap();
		CreateShaderBindingTable();
	}

	return S_OK;
}


// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files 
// --------------------------------------------------------
void Game::LoadShaders()
{

	//this describes the type of constant buffer and which register to map the data to
	CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
	CD3DX12_ROOT_PARAMETER1 rootParams[EntityRootIndices::EntityNumRootIndices]; // specifies the descriptor table
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParams[EntityRootIndices::EntityVertexCBV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[EntityRootIndices::EntityIndex].InitAsConstants(1, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityPixelCBV].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityMaterials].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityMaterialIndex].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityEnvironmentSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityLTCSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplers[0].Init(0);
	staticSamplers[1].Init(1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

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
	ComPtr<ID3DBlob> sssPixelShaderBlob;
	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShaderPBR.cso", pbrPixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"SubsurfaceScatteringPS.cso", sssPixelShaderBlob.GetAddressOf()));

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

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC sssDescPBR = {};
	sssDescPBR.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	sssDescPBR.pRootSignature = rootSignature.Get();
	sssDescPBR.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	sssDescPBR.PS = CD3DX12_SHADER_BYTECODE(sssPixelShaderBlob.Get());
	////psoPBRDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//psoDePBRsc.DepthStencilState.StencilEnable = FALSE;
	sssDescPBR.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	sssDescPBR.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	sssDescPBR.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	sssDescPBR.SampleMask = UINT_MAX;
	sssDescPBR.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	sssDescPBR.NumRenderTargets = 1;
	sssDescPBR.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	sssDescPBR.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	sssDescPBR.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&sssDescPBR, IID_PPV_ARGS(sssPipelineState.GetAddressOf())));

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

	//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	particleDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
	particleRootParams[0].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
	//
	particleRootParams[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	particleRootParams[2].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
	psoDescParticle.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDescParticle.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDescParticle.DepthStencilState.DepthEnable = true;
	psoDescParticle.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDescParticle.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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
	psoDescParticle.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1026 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightingConstantBufferResource.GetAddressOf())
	));

	ZeroMemory(&lightData, sizeof(lightData));
	ZeroMemory(&lightingData, sizeof(lightingData));

	lightData.light1.diffuse = XMFLOAT4(1, 1, 1, 1);
	lightData.light1.direction = XMFLOAT3(-1, -1, 0);
	lightData.light1.ambientColor = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
	lightData.light1.specularity = XMFLOAT4(1, 0, 0, 1);

	lightConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightCbufferBegin));
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	lightingData.lights[0].type = LIGHT_TYPE_DIR;
	lightingData.lights[0].direction = XMFLOAT3(-1, -1, 0);
	lightingData.lights[0].color = XMFLOAT3(1, 1, 1);
	lightingData.lights[0].intensity = 1;
	lightCount++;

	lightingData.lights[1].type = 3;
	lightingData.lights[1].color = XMFLOAT3(1, 1, 1);
	lightingData.lights[1].rectLight.height = 5;
	lightingData.lights[1].rectLight.width = 5;
	lightingData.lights[1].rectLight.rotY = 0;
	lightingData.lights[1].rectLight.rotZ = 0;
	lightingData.lights[1].rectLight.position = XMFLOAT3(0, -2, 0);
	lightingData.lights[1].intensity = 8;
	lightCount++;


	lightingConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightingCbufferBegin));
	memcpy(lightingCbufferBegin, &lightingData, sizeof(lightingData));

	UINT64 cbufferOffset = 0;
	mesh1 = std::make_shared<Mesh>("../../Assets/Models/sphere.obj", device, commandList);
	mesh2 = std::make_shared<Mesh>("../../Assets/Models/cube.obj", device, commandList);
	mesh3 = std::make_shared<Mesh>("../../Assets/Models/helix.obj", device, commandList);
	sharkMesh = std::make_shared<Mesh>("../../Assets/Models/bird2.obj", device, commandList);
	faceMesh = std::make_shared<Mesh>("../../Assets/Models/face.obj", device, commandList);
	skyDome = std::make_shared<Mesh>("../../Assets/Models/sky_dome.obj", device, commandList);
	std::shared_ptr<Mesh> rect = std::make_shared<Mesh>("../../Assets/Models/RectLight.obj", device, commandList);
	

	//creating the vertex buffer
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	float aspectRatio = static_cast<float>(width / height);

	//CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mainCPUDescriptorHandle, 0, cbvDescriptorSize);
	material1 = std::make_shared<Material>(device, commandQueue,mainBufferHeap, pbrPipelineState,rootSignature,
		L"../../Assets/Textures/GoldDiffuse.png", L"../../Assets/Textures/GoldNormal.png",
		L"../../Assets/Textures/Goldroughness.png",L"../../Assets/Textures/GoldMetallic.png");
	material2 = std::make_shared<Material>(device, commandQueue, mainBufferHeap, pbrPipelineState, rootSignature,
		L"../../Assets/Textures/LayeredDiffuse.png",L"../../Assets/Textures/LayeredNormal.png",
		L"../../Assets/Textures/LayeredRoughness.png", L"../../Assets/Textures/LayeredMetallic.png");

	std::shared_ptr<Material> material3 = std::make_shared<Material>(device, commandQueue, mainBufferHeap, pbrPipelineState, rootSignature,
		L"../../Assets/Textures/BronzeDiffuse.png", L"../../Assets/Textures/BronzeNormal.png",
		L"../../Assets/Textures/BronzeRoughness.png", L"../../Assets/Textures/BronzeMetallic.png");

	std::shared_ptr<Material> material4 = std::make_shared<Material>(device, commandQueue, mainBufferHeap, sssPipelineState, rootSignature,
		L"../../Assets/Textures/Head_Diffuse.png", L"../../Assets/Textures/Head_Normal.png");

	std::shared_ptr<Material> material5 = std::make_shared<Material>(device, commandQueue, mainBufferHeap, sssPipelineState, rootSignature,
		L"../../Assets/Textures/GoldMetallic.png", L"../../Assets/Textures/GoldNormal.png", L"../../Assets/Textures/DefaultRoughness.png", 
		L"../../Assets/Textures/LayeredMetallic.png");

	materials.emplace_back(material1);
	materials.emplace_back(material2);
	materials.emplace_back(material3);
	materials.emplace_back(material4);
	materials.emplace_back(material5);


	entity1 = std::make_shared<Entity>(mesh2,material1, registry);
	entity2 = std::make_shared<Entity>(mesh1,material1, registry);
	entity3 = std::make_shared<Entity>(mesh1,material2, registry);
	entity4 = std::make_shared<Entity>(mesh3,material1, registry);
	entity6 = std::make_shared<Entity>(mesh2, material5, registry);
	
	

	entity1->SetPosition(XMFLOAT3(0, -10, 1.5f));
	entity1->SetScale(XMFLOAT3(10, 10, 10));
	entity2->SetPosition(XMFLOAT3(1, 0, 1.0f));
	entity3->SetPosition(XMFLOAT3(-3, 0, 1.f));
	entity4->SetPosition(XMFLOAT3(8, -8, 1.f));
	entity6->SetPosition(lightingData.lights[1].rectLight.position);
	entity6->SetScale(XMFLOAT3(5, 5, 0.01));

	auto initialRot = entity6->GetRotation();
	XMVECTOR finalRot = XMQuaternionRotationRollPitchYaw(0, lightingData.lights[1].rectLight.rotY * 3.14159 / 180, lightingData.lights[1].rectLight.rotZ * 3.14159 / 180);
	XMStoreFloat4(&initialRot, finalRot);
	entity6->SetRotation(initialRot);


	entity1->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity2->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity3->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity4->PrepareConstantBuffers(device,residencyManager,residencySet);
	entity6->PrepareConstantBuffers(device, residencyManager, residencySet);


	entities.emplace_back(entity1);
	entities.emplace_back(entity2);
	entities.emplace_back(entity3);
	entities.emplace_back(entity4);
	entities.emplace_back(entity6);
	entities.emplace_back(std::make_shared<Entity>(mesh3, material2, registry));


	entities[entities.size() - 1]->SetPosition(XMFLOAT3(0, 2, 0));
	entities[entities.size() - 1]->PrepareConstantBuffers(device,residencyManager,residencySet);

	//for (int i = 0; i < 10; i++)
	//{
	//	flockers.emplace_back(std::make_shared<Entity>(sharkMesh, material2, registry));
	//	flockers[i]->PrepareConstantBuffers(device, residencyManager, residencySet);
	//	const auto enttID = flockers[i]->GetEntityID();
	//
	//	flockers[i]->SetPosition(XMFLOAT3(static_cast<float>(i + 6), static_cast<float>(i - 6), 0.f));
	//	//registry.assign<Flocker>(enttID, XMFLOAT3(i+2,i-2,0), 2 ,XMFLOAT3(0,0,0), 10,XMFLOAT3(0,0,0),1);
	//	auto valid = registry.valid(enttID);
	//
	//	auto &flocker = registry.assign<Flocker>(enttID);
	//	flocker.pos = XMFLOAT3(static_cast<float>(i + 6), static_cast<float>(i - 6), 0.f);
	//	flocker.vel = XMFLOAT3(0, 0, 0);
	//	flocker.acceleration = XMFLOAT3(0, 0, 0);
	//	flocker.mass = 2;
	//	flocker.maxSpeed = 2;
	//	flocker.safeDistance = 1;
	//}

}

void Game::CreateEnvironment()
{
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	CD3DX12_ROOT_PARAMETER1 rootParams[EnvironmentRootIndices::EnvironmentNumRootIndices]; // specifies the descriptor table
	rootParams[EnvironmentRootIndices::EnvironmentVertexCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[EnvironmentRootIndices::EnvironmentTextureSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EnvironmentRootIndices::EnvironmentRoughness].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EnvironmentRootIndices::EnvironmentTexturesData].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[EnvironmentRootIndices::EnvironmentFaceIndices].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);


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
	skybox = std::make_shared<Skybox>(L"../../Assets/Textures/skybox1.dds", mesh2, skyboxPSO, skyboxRootSignature, device, commandQueue, mainBufferHeap);

	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleAllocator.Get(), skyboxPSO.Get(), IID_PPV_ARGS(skyboxBundle.GetAddressOf())));

	//loading the shaders for image based lighting
	
	//irradiance map calculations


	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"IrradianceMapPS.cso", pixelShaderBlob.GetAddressOf()));
	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC irradiancePsoDesc = {};
	irradiancePsoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	irradiancePsoDesc.pRootSignature = skyboxRootSignature.Get();
	irradiancePsoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	irradiancePsoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	//irradiancePsoDescDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//irradiancePsoDescsc.DepthStencilState.StencilEnable = FALSE;
	irradiancePsoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	irradiancePsoDesc.DepthStencilState.StencilEnable = FALSE;
	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	irradiancePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	irradiancePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	irradiancePsoDesc.SampleMask = UINT_MAX;
	irradiancePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	irradiancePsoDesc.NumRenderTargets = 1;
	irradiancePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	irradiancePsoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	irradiancePsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&irradiancePsoDesc, IID_PPV_ARGS(irradiencePSO.GetAddressOf())));

	//prefilteredmap
	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PrefilteredMapPS.cso", pixelShaderBlob.GetAddressOf()));
	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC prefiltermapPSODesc = {};
	prefiltermapPSODesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	prefiltermapPSODesc.pRootSignature = skyboxRootSignature.Get();
	prefiltermapPSODesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	prefiltermapPSODesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	//prefiltermapPSODescscDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	//prefiltermapPSODescscsc.DepthStencilState.StencilEnable = FALSE;
	prefiltermapPSODesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	prefiltermapPSODesc.DepthStencilState.StencilEnable = FALSE;
	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	prefiltermapPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	prefiltermapPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	prefiltermapPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	prefiltermapPSODesc.SampleMask = UINT_MAX;
	prefiltermapPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	prefiltermapPSODesc.NumRenderTargets = 1;
	prefiltermapPSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	prefiltermapPSODesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	prefiltermapPSODesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&prefiltermapPSODesc, IID_PPV_ARGS(prefilteredMapPSO.GetAddressOf())));

	//BRDF LUT

	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"IntegrationBrdfPS.cso", pixelShaderBlob.GetAddressOf()));
	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC integrationBRDFDesc = {};
	integrationBRDFDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	integrationBRDFDesc.pRootSignature = skyboxRootSignature.Get();
	integrationBRDFDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	integrationBRDFDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	integrationBRDFDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
	integrationBRDFDesc.DepthStencilState.StencilEnable = FALSE;
	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	integrationBRDFDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	integrationBRDFDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	integrationBRDFDesc.SampleMask = UINT_MAX;
	integrationBRDFDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	integrationBRDFDesc.NumRenderTargets = 1;
	integrationBRDFDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	integrationBRDFDesc.RTVFormats[0] = DXGI_FORMAT_R32G32_FLOAT;
	integrationBRDFDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&integrationBRDFDesc, IID_PPV_ARGS(brdfLUTPSO.GetAddressOf())));



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
	sbtGenerator.AddRayGenerationProgram(L"RayGen", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress() });
	sbtGenerator.AddMissProgram(L"Miss", {heapPointer});
	for (int i = 0; i < bottomLevelBufferInstances.size(); i++)
	{
		UINT materialIndex = entities[i]->GetMaterialIndex();
		auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
		sbtGenerator.AddHitGroup(L"HitGroup", { (void*)entities[i]->GetMesh()->GetVertexBufferResource()->GetGPUVirtualAddress(),(void*)lightingConstantBufferResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr});
		sbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
	}

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


	//creating the sbt for the gbuffer
	{
		//shader binding tables define the raygen, miss, and hit group shaders
		//these resources are interpreted by the shader
		GBsbtGenerator.Reset();
	
		//getting the descriptor heap handle
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = rtDescriptorHeap.GetHeap()->GetGPUDescriptorHandleForHeapStart();
	
		//reinterpreting the above pointer as a void pointer
		auto heapPointer = reinterpret_cast<UINT64*>(gpuHandle.ptr);
	
		//the ray generation shader needs external data therefore it needs the pointer to the heap
		//the miss and hit group shaders don't have any data
		GBsbtGenerator.AddRayGenerationProgram(L"GBufferRayGen", { heapPointer });
		GBsbtGenerator.AddMissProgram(L"GBufferMiss", { heapPointer });
		for (int i = 0; i < bottomLevelBufferInstances.size(); i++)
		{
			UINT materialIndex = entities[i]->GetMaterialIndex();
			auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
			GBsbtGenerator.AddHitGroup(L"GBufferHitGroup", { (void*)entities[i]->GetMesh()->GetVertexBufferResourceAndCount().first.Get()->GetGPUVirtualAddress(),(void*)lightingConstantBufferResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
			GBsbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

		}

	
		//compute the size of the GBsbt
		UINT32 GBsbtSize = GBsbtGenerator.ComputeSBTSize();
	
		//upload heap for the GBsbt
		GBsbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), GBsbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
	
		if (!GBsbtResource)
			throw std::logic_error("Could not create GBsbt resource");
	
		//compile the GBsbt from the above info
		GBsbtGenerator.Generate(GBsbtResource.Get(), GBrtStateObjectProps.Get());
	}
}


//function that initializes the optix denoiser
void Game::InitOptix()
{
	//optixContext = optix::Context::create();
	//inputBuffer = optixContext->createBuffer(RT_BUFFER_INPUT_OUTPUT,RT_FORMAT_FLOAT4,width,height);
	//outBuffer = optixContext->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, width, height);
	//normalBuffer = optixContext->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, width, height);
	//albedoBuffer = optixContext->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, width, height);
	//
	//denoiserStage = optixContext->createBuiltinPostProcessingStage("DLDenoiser");
	////denoiserStage->declareVariable("inputBuffer")->set(inputBuffer);
	////denoiserStage->declareVariable("outputBuffer")->set(outBuffer);
	////denoiserStage->declareVariable("albedoBuffer")->set(albedoBuffer);
	////denoiserStage->declareVariable("normalBuffer")->set(normalBuffer);
	////denoiserStage->declareVariable("blend");
	////denoiserStage->declareVariable("hdr");
	//
	////optix::Variable optixVar;
	////optixVar = denoiserStage->queryVariable("blend");
	////optixVar->setFloat(0.05f);
	////optixVar = denoiserStage->queryVariable("hdr");
	////optixVar->setUint(1);
	//
	////add the denoiser to the command list
	//optiXCommandList = optixContext->createCommandList();
	////optiXCommandList->appendPostprocessingStage(denoiserStage, width, height);
	//optiXCommandList->finalize();
	//
	//optixContext->validate();
	//optixContext->compile();
	//
	//optiXCommandList->execute();
	//
	//readbackInputBuffer = CreateRBBuffer(readbackInputBuffer.Get(), device.Get(), width * height * sizeof(float) * 4);
	//readbackNormalBuffer = CreateRBBuffer(readbackNormalBuffer.Get(), device.Get(), width * height * sizeof(float) * 4);
	//readbackAlbedoBuffer = CreateRBBuffer(readbackAlbedoBuffer.Get(), device.Get(), width * height * sizeof(float) * 4);


}

void Game::SetupDenoising()
{
}

void Game::DenoiseOutput()
{
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

void Game::CreateTopLevelAS(const std::vector<EntityInstance>& instances)
{
	for (int i = 0; i < instances.size(); i++)
	{
		topLevelAsGenerator.AddInstance(instances[i].bottomLevelBuffer.Get(), instances[i].modelMatrix, static_cast<UINT>(i), static_cast<UINT>(i*2),D3D12_RAYTRACING_INSTANCE_FLAG_NONE,0xFF);
	}

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
	std::vector<AccelerationStructureBuffers> bottomLevelBuffers;

	for (int i = 0; i < entities.size(); i++)
	{
		AccelerationStructureBuffers blasBuffer = CreateBottomLevelAS({ entities[i]->GetMesh()->GetVertexBufferResourceAndCount() });
		bottomLevelBuffers.emplace_back(blasBuffer);
	}

	//create only one instance for now

	//instances.emplace_back(std::pair<ComPtr<ID3D12Resource>, XMFLOAT4X4>(bottomLevelBuffers.pResult, entities[3]->GetModelMatrix()));

	for (int i = 0; i < bottomLevelBuffers.size(); i++)
	{
		EntityInstance instance = {i ,bottomLevelBuffers[i].pResult, entities[i]->GetRawModelMatrix() };
		bottomLevelBufferInstances.emplace_back(instance);
	}

	//bottomLevelBufferInstances = { {bottomLevelBuffer.pResult, entities[3]->GetRawModelMatrix()},{bottomLevelBuffer.pResult, XMMatrixTranslation(-1,3,20)},{planeBottomLevelBuffer.pResult,entities[0]->GetRawModelMatrix()} };
	CreateTopLevelAS(bottomLevelBufferInstances);

	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForPreviousFrame();

	ThrowIfFailed(
		commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

}

ComPtr<ID3D12RootSignature> Game::CreateRayGenRootSignature()
{
	//this ray generation shader is getting and output texture and an acceleration structure
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({ 
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTOutputTexture}, 
		{1,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTDiffuseTexture},
		{2,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTPositionTexture},
		{3,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTNormalTexture},
		{4,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTAlbedoTexture},
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,RaytracingHeapRangesIndices::RTAccelerationStruct},
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,RaytracingHeapRangesIndices::RTCameraData}
	});
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);

	//rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0, 0);

	return rsc.Generate(device.Get(), true);
}

ComPtr<ID3D12RootSignature> Game::CreateMissRootSignature()
{
	//the miss signature only has a payload
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({ { 0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,RaytracingHeapRangesIndices::RTMissTexture } });
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc;
	samplerDesc.Init(0);
	rsc.AddStaticSamplers(samplerDesc);
	return rsc.Generate(device.Get(), true);
}

ComPtr<ID3D12RootSignature> Game::CreateClosestHitRootSignature()
{
	//the hit signature only has a payload
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
	rsc.AddHeapRangesParameter({ 
		{0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RaytracingHeapRangesIndices::RTAccelerationStruct},
		{0, 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RaytracingHeapRangesIndices::RTMaterials}
	});
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, 0, 0, 1);
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc;
	samplerDesc.Init(0);
	rsc.AddStaticSamplers(samplerDesc);
	//rsc.AddHeapRangesParameter()
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

	resDes.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes,D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtNormals.resource.GetAddressOf())));
	rtNormals.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtNormals.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtDiffuse.resource.GetAddressOf())));
	rtDiffuse.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtDiffuse.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtPosition.resource.GetAddressOf())));
	rtPosition.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtPosition.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtAlbedo.resource.GetAddressOf())));
	rtAlbedo.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	rtAlbedo.resourceType = RESOURCE_TYPE_UAV;
}

void Game::CreateRaytracingDescriptorHeap()
{
	//creating the descriptor heap, it will contain two descriptors
	//one for the UAV output and an SRV for the acceleration structure
	rtDescriptorHeap.Create(device, 10000, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	rtDescriptorHeap.CreateDescriptor(rtOutPut, rtOutPut.resourceType, device);
	rtDescriptorHeap.CreateDescriptor(rtDiffuse, rtDiffuse.resourceType, device);
	rtDescriptorHeap.CreateDescriptor(rtPosition, rtPosition.resourceType, device);
	rtDescriptorHeap.CreateDescriptor(rtNormals, rtNormals.resourceType, device);
	rtDescriptorHeap.CreateDescriptor(rtAlbedo, rtAlbedo.resourceType, device);


	//initializing the camera buffer used for raytracing
	//ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		//D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(cameraData.resource.GetAddressOf())));

	rtDescriptorHeap.CreateRaytracingAccelerationStructureDescriptor(device, rtOutPut, topLevelAsBuffers);
	rtDescriptorHeap.CreateDescriptor(cameraData, RESOURCE_TYPE_CBV, device, sizeof(RayTraceCameraData));
	rtDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/skybox1.dds",skyboxTexResource,RESOURCE_TYPE_SRV,device,commandQueue,TEXTURE_TYPE_DDS, true);

	static UINT numStaticResources = 0;
	for (int i = 0; i < materials.size(); i++)
	{
		auto cpuHandle = rtDescriptorHeap.GetCPUHandle(rtDescriptorHeap.GetLastResourceIndex());
		auto otherCPUHandle = materials[i]->GetDescriptorHeap().GetCPUHandle(0);
		numStaticResources += 4;
		rtDescriptorHeap.IncrementLastResourceIndex(4);
		device->CopyDescriptorsSimple(4, cpuHandle, otherCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

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
	pipeline.SetMaxPayloadSize(20 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(30);

	//creating the state obj
	rtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(rtStateObject->QueryInterface(IID_PPV_ARGS(rtStateObjectProps.GetAddressOf())));
}

void Game::CreateGbufferRaytracingPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	GBrayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../GBufferRayGen.hlsl");
	GBmissLib = nv_helpers_dx12::CompileShaderLibrary(L"../../GBufferMiss.hlsl");
	GBhitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../GbufferHit.hlsl");

	//add the libraies to pipeliene
	pipeline.AddLibrary(GBrayGenLib.Get(), { L"GBufferRayGen" });
	pipeline.AddLibrary(GBmissLib.Get(), { L"GBufferMiss" });
	pipeline.AddLibrary(GBhitLib.Get(), { L"GBufferClosestHit",L"GBufferPlaneClosestHit" });

	//creating the root signatures
	rayGenRootSig = CreateRayGenRootSignature();
	missRootSig = CreateMissRootSignature();
	closestHitRootSignature = CreateClosestHitRootSignature();


	shadowRayLib = nv_helpers_dx12::CompileShaderLibrary(L"../../ShadowRay.hlsl");
	pipeline.AddLibrary(shadowRayLib.Get(), { L"ShadowClosestHit",L"ShadowMiss" });
	shadowRootSig = CreateClosestHitRootSignature();

	//adding a hit group to the pipeline
	pipeline.AddHitGroup(L"GBufferHitGroup", L"GBufferClosestHit");
	pipeline.AddHitGroup(L"GBufferPlaneHitGroup", L"GBufferPlaneClosestHit");
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");


	//associating the root signatures with the shaders
	//shaders can share root signatures
	pipeline.AddRootSignatureAssociation(rayGenRootSig.Get(), { L"GBufferRayGen" });
	pipeline.AddRootSignatureAssociation(missRootSig.Get(), { L"GBufferMiss",L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"GBufferHitGroup" ,L"GBufferPlaneHitGroup",L"ShadowHitGroup" }); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
	//pipeline.AddRootSignatureAssociation(shadowRootSig.Get(), { L"ShadowHitGroup" });

	//payload size defines the maximum size of the data carried by the rays
	pipeline.SetMaxPayloadSize(20 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(30);

	//creating the state obj
	gbufferStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(gbufferStateObject->QueryInterface(IID_PPV_ARGS(GBrtStateObjectProps.GetAddressOf())));
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

	if (GetAsyncKeyState('R')&&!rtToggle) 
	{
		rtToggle = true;

		if(isRaytracingAllowed)
			raster = !raster;
	}

	else if (GetAsyncKeyState('R') == 0)
	{
		rtToggle = false;
	}

	static bool subsurfaceToggle = false;

	if (GetAsyncKeyState('K') && !subsurfaceToggle)
	{
		enableSSS = !enableSSS;
		subsurfaceToggle = true;
	}

	else if (GetAsyncKeyState('K') == 0)
	{
		subsurfaceToggle = false;
	}


	lightData.cameraPosition = mainCamera->GetPosition();
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	lightingData.lights[1].rectLight.rotY += 4 * deltaTime;

	auto initialRot = entity6->GetRotation();
	XMVECTOR finalRot = XMQuaternionRotationRollPitchYaw(0, lightingData.lights[1].rectLight.rotY * 3.14159 / 180, lightingData.lights[1].rectLight.rotZ * 3.14159 / 180);	
	XMStoreFloat4(&initialRot, finalRot);
	entity6->SetRotation(initialRot);

	lightingData.cameraPosition = mainCamera->GetPosition();
	lightingData.lightCount = lightCount;
	memcpy(lightingCbufferBegin, &lightingData, sizeof(lightingData));

	if (isRaytracingAllowed)
	{

		rtCamera.view = mainCamera->GetViewMatrix();
		rtCamera.proj = mainCamera->GetProjectionMatrix();
		XMMATRIX viewTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.view));
		XMStoreFloat4x4(&rtCamera.iView, XMMatrixTranspose(XMMatrixInverse(nullptr, viewTranspose)));
		XMMATRIX projTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.proj));
		XMStoreFloat4x4(&rtCamera.iProj, XMMatrixTranspose(XMMatrixInverse(nullptr, projTranspose)));
		//
		memcpy(cameraBufferBegin, &rtCamera, sizeof(rtCamera));
	}

	emitter1->UpdateParticles(deltaTime, totalTime);

	//FlockingSystem::FlockerSystem(registry, flockers, deltaTime);

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
		commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityMaterials, gpuCBVSRVUAVHandle);
		//gpuCBVSRVUAVHandle.Offset(gpuHeapRingBuffer->GetBeginningStaticResourceOffset());
		gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetStaticDescriptorOffset();
		//commandList->SetGraphicsRootDescriptorTable(0, gpuHeapRingBuffer->GetStaticDescriptorOffset());

		for (UINT i = 0; i < entities.size(); i++)
		{
			commandList->SetGraphicsRootSignature(entities[i]->GetRootSignature().Get());
			commandList->SetPipelineState(entities[i]->GetPipelineState().Get());

			entities[i]->PrepareMaterial(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());

			gpuHeapRingBuffer->AddDescriptor(device, 1, entities[i]->GetDescriptorHeap(), 0);

			commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityVertexCBV, gpuHeapRingBuffer->GetDynamicResourceOffset());
			commandList->SetGraphicsRoot32BitConstant(EntityRootIndices::EntityIndex, enableSSS, 0);
			commandList->SetGraphicsRoot32BitConstant(EntityRootIndices::EntityMaterialIndex, entities[i]->GetMaterialIndex(), 0);
			commandList->SetGraphicsRootConstantBufferView(EntityRootIndices::EntityPixelCBV, lightingConstantBufferResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityEnvironmentSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->environmentTexturesIndex));
			commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityLTCSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(ltcLUT.heapOffset));

			D3D12_VERTEX_BUFFER_VIEW vertexBuffer = entities[i]->GetMesh()->GetVertexBuffer();
			auto indexBuffer = entities[i]->GetMesh()->GetIndexBuffer();

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
		//back buffer will now be used to present

		emitter1->Draw(commandList, gpuHeapRingBuffer, mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), totalTime);
	}

	else if(!raster&&isRaytracingAllowed)
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

		CreateGBufferRays();

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

void Game::CreateGBufferRays()
{
	//creating a dispatch rays description
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//raygeneration location
	desc.RayGenerationShaderRecord.StartAddress = GBsbtResource->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
	desc.RayGenerationShaderRecord.SizeInBytes = GBsbtGenerator.GetRayGenSectionSize();

	//miss shaders
	desc.MissShaderTable.StartAddress = GBsbtResource->GetGPUVirtualAddress() + GBsbtGenerator.GetRayGenSectionSize();
	desc.MissShaderTable.SizeInBytes = GBsbtGenerator.GetMissSectionSize();
	//desc.MissShaderTable.StartAddress = (desc.MissShaderTable.StartAddress + 63) & ~63;
	desc.MissShaderTable.StrideInBytes = GBsbtGenerator.GetMissEntrySize();

	//hit groups
	desc.HitGroupTable.StartAddress = GBsbtResource->GetGPUVirtualAddress() + GBsbtGenerator.GetRayGenSectionSize() + GBsbtGenerator.GetMissSectionSize();
	//desc.HitGroupTable.StartAddress = (desc.HitGroupTable.StartAddress + 63) & ~63;
	desc.HitGroupTable.SizeInBytes = GBsbtGenerator.GetHitGroupSectionSize();
	desc.HitGroupTable.StrideInBytes = GBsbtGenerator.GetHitGroupEntrySize();

	//scene description
	desc.Height = height;
	desc.Width = width;
	desc.Depth = 1;

	commandList->SetPipelineState1(gbufferStateObject.Get());
	commandList->DispatchRays(&desc);
}

void Game::CreateLTCTexture()
{
	//D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	//heapDesc.NumDescriptors = 1;
	//heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	//
	//ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(ltcDescriptorHeap.GetAddressOf())));
	//
	//// Create the texture.
	//
	//// Describe and create a Texture2D.
	//D3D12_RESOURCE_DESC textureDesc = {};
	//textureDesc.MipLevels = 1;
	//textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	//textureDesc.Width = 64;
	//textureDesc.Height = 64;
	//textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//textureDesc.DepthOrArraySize = 1;
	//textureDesc.SampleDesc.Count = 1;
	//textureDesc.SampleDesc.Quality = 0;
	//textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//
	//ThrowIfFailed(device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	//	D3D12_HEAP_FLAG_NONE,
	//	&textureDesc,
	//	D3D12_RESOURCE_STATE_COPY_DEST,
	//	nullptr,
	//	IID_PPV_ARGS(ltcLUT.resource.GetAddressOf())));
	//
	//const UINT64 uploadBufferSize = GetRequiredIntermediateSize(ltcLUT.resource.Get(), 0, 1);
	//
	//// Create the GPU upload buffer.
	//ThrowIfFailed(device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//	D3D12_HEAP_FLAG_NONE,
	//	&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&ltcTextureUploadHeap)));
	//
	//// Copy data to the intermediate upload heap and then schedule a copy 
	//// from the upload heap to the Texture2D
	//
	//D3D12_SUBRESOURCE_DATA textureData = {};
	//textureData.pData = &ltc2[0];
	//textureData.RowPitch = 64 * 4;
	//textureData.SlicePitch = textureData.RowPitch * 64;
	//
	//UpdateSubresources<1>(commandList.Get(), ltcLUT.resource.Get(), ltcTextureUploadHeap.Get(), 0, 0, 1, &textureData);
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ltcLUT.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	//
	//// Describe and create a SRV for the texture.
	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//srvDesc.Format = textureDesc.Format;
	//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	//srvDesc.Texture2D.MipLevels = 1;
	//
	//device->CreateShaderResourceView(ltcLUT.resource.Get(),&srvDesc,ltcDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	ltcDescriptorHeap.Create(device, 3, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ltcDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/ltc_mat.png", ltcLUT, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT, false);
	ltcDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/ltc_amp.png", ltcLUT2, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT, false);
	ltcDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/ltc_1.png", ltcTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT, false);


	if (gpuHeapRingBuffer != nullptr)
	{
	
		gpuHeapRingBuffer->AllocateStaticDescriptors(device, 3, ltcDescriptorHeap);
		ltcLUT.heapOffset = gpuHeapRingBuffer->GetNumStaticResources() - 3;
	}
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