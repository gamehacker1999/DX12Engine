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
		1920,			   // Width of the window's client area
		1080, 
		1280, 
		720)			   // Height of the window's client area
{

	prevMousePos = { 0,0 };

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif
	gizmoMode = ImGuizmo::TRANSLATE;
	pickingIndex = -1;
	constantBufferBegin = nullptr;
	cameraBufferBegin = 0;
	lightCbufferBegin = 0;
	lightingCbufferBegin = 0;
	lightBufferBegin = 0;
	lightCullingExternBegin = 0;
	previousBuffer = nullptr;
	lightCount = 0;
	raster = true;
	inlineRaytracing = true;

	memset(fenceValues, 0, sizeof(UINT64) * frameIndex);
	memset(&lightingData, 0, sizeof(LightingData));
	memset(&lightCullingExternData, 0, sizeof(LightCullingExternalData));

	isRaytracingAllowed = false;
	rtToggle = true;
	doRestir = false;
	doRestirGI = false;
	restirSpatialReuse = false;
	enableSSS = false;
	visibleLightIndices = nullptr;
	visibleLightIndicesResource = 0;
	entityManipulated = false;
	keys.Reset();
	mouseButtons.Reset();
	entityNames.resize(10000000);

	fogDensity = 1.f;
	lights = nullptr;

}

Game::~Game()
{
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
	residencyManager.Destroy();

	if (visibleLightIndices != nullptr)
	{
		delete[] visibleLightIndices;
	}

	if (lights != nullptr)
	{
		delete[] lights;
	}

	for (int i = 0; i < flockers.size(); i++)
	{
		registry.destroy(flockers[i]->GetEntityID());
	}


	delete[] jitters;

	dynamicBufferRing.OnDestroy();

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
	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1)
		isRaytracingAllowed = false;
	else
		isRaytracingAllowed = true;

	frameIndex = this->swapChain->GetCurrentBackBufferIndex();

	HRESULT hr;

	numFrames = -1;

	jitters = GenerateHaltonJitters();

	prevJitters.x = 0;
	prevJitters.y = 0;

	currentJitters.x = 0;
	currentJitters.y = 0;

	InitComputeEngine();


	//create synchronization object and wait till the objects have been passed to the gpu
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(computeFence.GetAddressOf())));
	fenceValues[frameIndex]++;
	//fence event handle for synchronization
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	for (size_t n = 0; n < frameCount; n++)
	{

		hr = (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
		if (FAILED(hr)) return hr;

		//creating the compute command allocators
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(computeCommandAllocator[n].GetAddressOf())));
	}

	//create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), pipelineState.Get(),
		IID_PPV_ARGS(commandList.GetAddressOf())));
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, computeCommandAllocator[frameIndex].Get(), computePipelineState.Get(), IID_PPV_ARGS(&computeCommandList)));

	InitResources(device, commandList, computeCommandList, commandQueue, commandAllocators,
		computeCommandQueue, computeCommandAllocator, fence, computeFence, fenceValues, fenceEvent);

	// Create descriptor heaps.
	{

		ThrowIfFailed(rtvDescriptorHeap.Create(frameCount + 100, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

		ThrowIfFailed(dsDescriptorHeap.Create(10, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

	}


	// Create frame resources.
	{

		// Create a RTV for each frame.
		for (UINT n = 0; n < frameCount; n++)
		{
			hr = (this->swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n].resource)));
			if (FAILED(hr)) return hr;
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			device->CreateRenderTargetView(renderTargets[n].resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(n));
			rtvDescriptorHeap.IncrementLastResourceIndex(1);
			//rtvHandle.Offset(1, rtvDescriptorSize);

		}

	}


	gpuHeapRingBuffer = std::make_shared<GPUHeapRingBuffer>();

	//residencyManager = std::make_shared<D3DX12Residency::ResidencyManager>();
	residencyManager.Initialize(device.Get(), 0, adapter.Get(), frameCount);
	residencySet = std::shared_ptr<D3DX12Residency::ResidencySet>(residencyManager.CreateResidencySet());

	residencySet->Open();

	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer));

	//Initializing the scene ring buffer
	dynamicBufferRing.OnCreate(device, 3, 400 * 1024 * 1024);


	//creating a final render target
	D3D12_RESOURCE_DESC renderTexureDesc = {};
	renderTexureDesc.Width = renderWidth;
	renderTexureDesc.Height = renderHeight;
	renderTexureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	renderTexureDesc.DepthOrArraySize = renderTargets[0].resource->GetDesc().DepthOrArraySize;
	renderTexureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	renderTexureDesc.MipLevels = renderTargets[0].resource->GetDesc().MipLevels;
	renderTexureDesc.SampleDesc.Quality = 0;
	renderTexureDesc.SampleDesc.Count = 1;
	renderTexureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// Background color (Cornflower Blue in this case) for clearing
	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };
	FLOAT black[4] = { 0.0f, 0.0f, 0.f, 1.0f };


	D3D12_CLEAR_VALUE rtvClearVal = {};
	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	rtvClearVal.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(finalRenderTarget.resource.GetAddressOf())
	));

	finalRenderTarget.resource->SetName(L"final target");

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&rtvClearVal,
		IID_PPV_ARGS(rtCombineOutput.resource.GetAddressOf())
	));

	rtCombineOutput.resource->SetName(L"rt combine output");
	rtCombineOutput.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(taaOutput.resource.GetAddressOf())
	));

	taaOutput.resource->SetName(L"taa output");

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(blurOutput.resource.GetAddressOf())
	));

	blurOutput.resource->SetName(L"blur output");

	blurOutput.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(taaInput.resource.GetAddressOf())
	));

	taaInput.resource->SetName(L"taa input");
	taaInput.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(taaHistoryBuffer.resource.GetAddressOf())
	));

	taaHistoryBuffer.resource->SetName(L"taa history");

	rtvClearVal.Color[0] = black[0];
	rtvClearVal.Color[1] = black[1];
	rtvClearVal.Color[2] = black[2];
	rtvClearVal.Color[3] = black[3];
	rtvClearVal.Format = DXGI_FORMAT_R32G32_FLOAT;
	renderTexureDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	renderTexureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(velocityBuffer.resource.GetAddressOf())
	));

	velocityBuffer.resource->SetName(L"velocity");

	velocityBuffer.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	rtvClearVal.Color[0] = color[0];
	rtvClearVal.Color[1] = color[1];
	rtvClearVal.Color[2] = color[2];
	rtvClearVal.Color[3] = color[3];
	renderTexureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvClearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(tonemappingOutput.resource.GetAddressOf())
	));

	tonemappingOutput.resource->SetName(L"tonemapping output");
	tonemappingOutput.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	renderTexureDesc.Width = width;
	renderTexureDesc.Height = height;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&rtvClearVal,
		IID_PPV_ARGS(editorWindowTarget.resource.GetAddressOf())
	));

	editorWindowTarget.resource->SetName(L"editor window");
	editorWindowTarget.currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(sharpenOutput.resource.GetAddressOf())
	));

	sharpenOutput.resource->SetName(L"sharpen output");
	sharpenOutput.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(fxaaOutput.resource.GetAddressOf())
	));

	fxaaOutput.resource->SetName(L"fxaa output");
	fxaaOutput.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(fsrIntermediateTexture.resource.GetAddressOf())
	));

	fsrIntermediateTexture.resource->SetName(L"fsr intermediate");
	fsrIntermediateTexture.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&renderTexureDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		&rtvClearVal,
		IID_PPV_ARGS(fsrOutputTexture.resource.GetAddressOf())
	));

	fsrOutputTexture.resource->SetName(L"fsr output");
	fsrOutputTexture.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;


	device->CreateRenderTargetView(finalRenderTarget.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	finalRenderTarget.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	finalRenderTarget.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(taaInput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	taaInput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	taaInput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(velocityBuffer.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	velocityBuffer.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	velocityBuffer.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(tonemappingOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	tonemappingOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	tonemappingOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(sharpenOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	sharpenOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	sharpenOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(taaHistoryBuffer.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	taaHistoryBuffer.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	taaHistoryBuffer.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(taaOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	taaOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	taaOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(rtCombineOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	rtCombineOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtCombineOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(fxaaOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	fxaaOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	fxaaOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);


	device->CreateRenderTargetView(blurOutput.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	blurOutput.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	blurOutput.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);

	device->CreateRenderTargetView(editorWindowTarget.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
	editorWindowTarget.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	editorWindowTarget.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
	rtvDescriptorHeap.IncrementLastResourceIndex(1);


	renderTargetSRVHeap.Create(100, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	renderTargetSRVHeap.CreateDescriptor(taaInput, RESOURCE_TYPE_SRV,0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(taaHistoryBuffer, RESOURCE_TYPE_SRV,  0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(velocityBuffer, RESOURCE_TYPE_SRV,0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(finalRenderTarget, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(taaOutput, RESOURCE_TYPE_SRV,0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(sharpenOutput, RESOURCE_TYPE_SRV, 0, width, height, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(tonemappingOutput, RESOURCE_TYPE_SRV,  0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(fxaaOutput, RESOURCE_TYPE_SRV,  0, width, height, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(rtCombineOutput, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(blurOutput, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(fsrIntermediateTexture, RESOURCE_TYPE_SRV, 0, width, height, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(fsrIntermediateTexture, RESOURCE_TYPE_UAV, 0, width, height, 0, 0);
	renderTargetSRVHeap.CreateDescriptor(fsrOutputTexture, RESOURCE_TYPE_SRV, 0, width, height, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(fsrOutputTexture, RESOURCE_TYPE_UAV, 0, width, height, 0, 0);

	renderTargetSRVHeap.CreateDescriptor(L"../../Assets/Textures/movemask2.bmp", blueNoiseTex, RESOURCE_TYPE_SRV, TEXTURE_TYPE_DEAULT);
	renderTargetSRVHeap.CreateDescriptor(L"../../Assets/Textures/movemask2.bmp", retargetTex, RESOURCE_TYPE_SRV, TEXTURE_TYPE_DEAULT);
	blueNoiseTex.resource->SetName(L"bluenoise");
	retargetTex.resource->SetName(L"Retarget");


	//optimized clear value for depth stencil buffer
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//creating the default resource heap for the depth stencil
	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, renderWidth, renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(depthStencilBuffer.resource.GetAddressOf())
	));

	dsDescriptorHeap.CreateDescriptor(depthStencilBuffer, RESOURCE_TYPE_DSV, 0, renderWidth, renderHeight);

	//creating the default resource heap for the depth stencil
	texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, renderWidth, renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(depthStencilBuffer2.resource.GetAddressOf())
	));

	dsDescriptorHeap.CreateDescriptor(depthStencilBuffer2, RESOURCE_TYPE_DSV, 0, renderWidth, renderHeight);

	D3D12_RESOURCE_DESC depthTexDesc = {};
	depthTexDesc.Width = renderWidth;
	depthTexDesc.Height = renderHeight;
	depthTexDesc.DepthOrArraySize = 1;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthTexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthTexDesc.Alignment = 0;

	//creating the default resource heap for the depth stencil
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&depthTexDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		&depthClearValue,
		IID_PPV_ARGS(depthTex.resource.GetAddressOf())
	));

	depthTex.currentState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	dsDescriptorHeap.CreateDescriptor(depthTex, RESOURCE_TYPE_DSV, 0, renderWidth, renderHeight);

	//depthDesc.Create(2, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//depthDesc.CreateDescriptor(depthTex, RESOURCE_TYPE_SRV, 0, 0, 0, 0, 1);


	//creating the skybox bundle
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(bundleAllocator.GetAddressOf())));

	//memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	//memcpy(constantBufferBegin+sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.

	mainCamera = std::make_shared<Camera>(XMFLOAT3(5.0f, 0.f, -3.0f), XMFLOAT3(-1.0f, 0.0f, 1.0f));

	mainCamera->CreateProjectionMatrix((float)renderWidth / renderHeight); //creating the camera projection matrix

	velocityBufferData.view = mainCamera->GetViewMatrix();
	velocityBufferData.projection = mainCamera->GetProjectionMatrix();
	velocityBufferData.prevProjection = mainCamera->GetProjectionMatrix();
	velocityBufferData.prevView = mainCamera->GetViewMatrix();

	taaData.inverseProjection = mainCamera->GetInverseProjection();
	taaData.inverseView = mainCamera->GetInverseView();
	taaData.prevView = mainCamera->GetViewMatrix();
	taaData.prevProjection = mainCamera->GetProjectionMatrix();

	LoadShaders();
	CreateMatrices();
	CreateBasicGeometry();
	CreateEnvironment();


	bmfrPreProcData.view = mainCamera->GetViewMatrix();
	bmfrPreProcData.proj = mainCamera->GetProjectionMatrix();
	bmfrPreProcData.prevView = mainCamera->GetViewMatrix();
	bmfrPreProcData.prevProj = mainCamera->GetProjectionMatrix();
	bmfrPreProcData.frame_number = -1;
	bmfrPreProcData.IMAGE_HEIGHT = renderHeight;
	bmfrPreProcData.IMAGE_WIDTH = renderWidth;

	memcpy(bmfrPreprocessBegin, &bmfrPreProcData, sizeof(bmfrPreProcData));


	if (isRaytracingAllowed)
		CreateAccelerationStructures();

	gpuHeapRingBuffer->AllocateStaticDescriptors(1, skybox->GetDescriptorHeap());
	skybox->skyboxTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;
	skybox->CreateEnvironment(skyboxRootSignature, skyboxRootSignature, irradiencePSO, prefilteredMapPSO, brdfLUTPSO, dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset));
	auto heap = skybox->GetEnvironmentHeap();
	gpuHeapRingBuffer->AllocateStaticDescriptors(3, heap);
	skybox->environmentTexturesIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1 - 2;

	CreateLTCTexture();

	flame = std::make_shared<RaymarchedVolume>(L"../../Assets/Textures/clouds.dds", mesh2, volumePSO,
		volumeRootSignature, mainBufferHeap);
	gpuHeapRingBuffer->AllocateStaticDescriptors(1, flame->GetDescriptorHeap());
	flame->volumeTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;

	emitter1 = std::make_shared<Emitter>(10000, //max particles
		100, //particles per second
		5.f, //lifetime
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
		particlesPSO, particleRootSig, L"../../Assets/Textures/particle.jpg");
	emitters.emplace_back(emitter1);

	for (int i = 0; i < emitters.size(); i++)
	{
		gpuHeapRingBuffer->AllocateStaticDescriptors(1, emitters[i]->GetDescriptor());
		emitters[i]->particleTextureIndex = gpuHeapRingBuffer->GetNumStaticResources() - 1;
	}

	//gpuHeapRingBuffer->AllocateStaticDescriptors(1, depthDesc);
	//depthTex.heapOffset = gpuHeapRingBuffer->GetNumStaticResources() - 1;

	ThrowIfFailed(commandList->Close());

	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };
	//skybox->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition());
	skyboxBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	skyboxBundle->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	/**/skyboxBundle->SetPipelineState(skybox->GetPipelineState().Get());
	skyboxBundle->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
	skyboxBundle->SetGraphicsRootConstantBufferView(EnvironmentRootIndices::EnvironmentVertexCBV, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
	skyboxBundle->SetGraphicsRootDescriptorTable(EnvironmentRootIndices::EnvironmentTextureSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->skyboxTextureIndex));
	auto vertexBuffer = skybox->GetMesh()->GetVertexBuffer();
	auto indexBuf = skybox->GetMesh()->GetIndexBuffer();
	skyboxBundle->IASetVertexBuffers(0, 1, &vertexBuffer);
	skyboxBundle->IASetIndexBuffer(&indexBuf);
	skyboxBundle->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	skyboxBundle->Close();


	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	residencySet->Close();

	WaitForPreviousFrame();
	commandAllocators[frameIndex]->Reset();

	ThrowIfFailed(
		commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

	if (isRaytracingAllowed)
	{
		CreateGbufferRaytracingPipeline();
		CreateRayTracingPipeline();
		CreateRaytracingOutputBuffer();
		CreateRaytracingDescriptorHeap();
		CreateShaderBindingTable();
	}
	InitializeGUI();

	SubmitComputeCommandList(computeCommandList, commandList);
	SubmitGraphicsCommandList(commandList);
	return S_OK;

}

void Game::InitComputeEngine()
{
	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	//Creating the compute command queue
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&computeCommandQueue)));


}

void Game::InitializeGUI()
{
	gpuHeapRingBuffer->GetDescriptorHeap().CreateDescriptor(editorWindowTarget, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	gpuHeapRingBuffer->IncrementNumStaticResources(1);
	ImGui_ImplDX12_Init(device.Get(), 3,
		DXGI_FORMAT_R8G8B8A8_UNORM, gpuHeapRingBuffer->GetDescriptorHeap().GetHeapPtr(),
		gpuHeapRingBuffer->GetDescriptorHeap().GetCPUHandle(gpuHeapRingBuffer->GetDescriptorHeap().GetLastResourceIndex()),
		gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(gpuHeapRingBuffer->GetDescriptorHeap().GetLastResourceIndex()));

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);
	// Our state

}


// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files 
// --------------------------------------------------------
void Game::LoadShaders()
{

	//this describes the type of constant buffer and which register to map the data to
	CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
	CD3DX12_ROOT_PARAMETER1 rootParams[EntityRootIndices::EntityNumRootIndices]; // specifies the descriptor table
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 3);
	rootParams[EntityRootIndices::EntityVertexCBV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[EntityRootIndices::EnableIndirectLighting].InitAsConstants(2, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityPixelCBV].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityMaterials].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityRoughnessVMFMapSRV].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityLightListSRV].InitAsShaderResourceView(0, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityLightIndices].InitAsShaderResourceView(1, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityMaterialIndex].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityEnvironmentSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::EntityLTCSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[EntityRootIndices::AccelerationStructureSRV].InitAsShaderResourceView(0, 4, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];//(0, D3D12_FILTER_ANISOTROPIC);
	staticSamplers[0].Init(0);
	staticSamplers[1].Init(1, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);

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
	ComPtr<ID3DBlob> depthPrePassPSBlob;
	ComPtr<ID3DBlob> depthPrePassVSBlob;
	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShaderPBR.cso", pbrPixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"SubsurfaceScatteringPS.cso", sssPixelShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"DepthPrePassPS.cso", depthPrePassPSBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"DepthPrePassVS.cso", depthPrePassVSBlob.GetAddressOf()));

	//input vertex layout, describes the semantics

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

	};

	{
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
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
	}

	{

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
		psoDescPBR.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDescPBR.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		psoDescPBR.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescPBR, IID_PPV_ARGS(pbrPipelineState.GetAddressOf())));
	}

	//creating a pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC sssDescPBR = {};
	sssDescPBR.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	sssDescPBR.pRootSignature = rootSignature.Get();
	sssDescPBR.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	sssDescPBR.PS = CD3DX12_SHADER_BYTECODE(sssPixelShaderBlob.Get());
	sssDescPBR.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	sssDescPBR.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	sssDescPBR.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	sssDescPBR.SampleMask = UINT_MAX;
	sssDescPBR.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	sssDescPBR.NumRenderTargets = 1;
	sssDescPBR.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	sssDescPBR.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	sssDescPBR.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&sssDescPBR, IID_PPV_ARGS(sssPipelineState.GetAddressOf())));

	//creating a depth prepass pipeline state
	D3D12_GRAPHICS_PIPELINE_STATE_DESC depthPrePassPSODesc = {};
	depthPrePassPSODesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
	depthPrePassPSODesc.pRootSignature = rootSignature.Get();
	depthPrePassPSODesc.VS = CD3DX12_SHADER_BYTECODE(depthPrePassVSBlob.Get());
	depthPrePassPSODesc.PS = CD3DX12_SHADER_BYTECODE(depthPrePassPSBlob.Get());
	depthPrePassPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthPrePassPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	depthPrePassPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	depthPrePassPSODesc.SampleMask = UINT_MAX;
	depthPrePassPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	depthPrePassPSODesc.NumRenderTargets = 0;
	depthPrePassPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthPrePassPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	depthPrePassPSODesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&depthPrePassPSODesc, IID_PPV_ARGS(depthPrePassPipelineState.GetAddressOf())));


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

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, volumeSignature.GetAddressOf(), volumeError.GetAddressOf()));

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
	psoDescVolume.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDescVolume.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDescVolume.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescVolume, IID_PPV_ARGS(volumePSO.GetAddressOf())));

	//creating particle root sig and pso

	{
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
		psoDescParticle.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDescParticle.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		psoDescParticle.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescParticle, IID_PPV_ARGS(particlesPSO.GetAddressOf())));
	}

	//Interior mapping
	{

		D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		};
		CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
		CD3DX12_ROOT_PARAMETER1 rootParams[InteriorMappingRootIndices::InteriorMappingNumParams];

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
		rootParams[InteriorMappingRootIndices::ExternDataVSCBV].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParams[InteriorMappingRootIndices::TextureArraySRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParams[InteriorMappingRootIndices::ExternDataPSCBV].InitAsConstants(6, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParams[InteriorMappingRootIndices::ExteriorTextureSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParams[InteriorMappingRootIndices::CapTextureSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParams[InteriorMappingRootIndices::SDFTextureSRV].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);


		ComPtr<ID3DBlob> interiorSignature;
		ComPtr<ID3DBlob> interiorError;

		CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];//(0, D3D12_FILTER_ANISOTROPIC);
		staticSamplers[0].Init(0);
		staticSamplers[1].Init(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);

		rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
			_countof(staticSamplers), staticSamplers, rootSignatureFlags);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
			interiorSignature.GetAddressOf(), interiorError.GetAddressOf()));

		ThrowIfFailed(device->CreateRootSignature(0, interiorSignature->GetBufferPointer(), interiorSignature->GetBufferSize(),
			IID_PPV_ARGS(interiorMappingRootSig.GetAddressOf())));

		ComPtr<ID3DBlob> interiorMappingVS;
		ComPtr<ID3DBlob> interiorMappingPS;

		ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", interiorMappingVS.GetAddressOf()));
		ThrowIfFailed(D3DReadFileToBlob(L"InteriorMappingPS.cso", interiorMappingPS.GetAddressOf()));

		//creating a pipeline state object
		D3D12_GRAPHICS_PIPELINE_STATE_DESC interiorMappingPSODesc = {};
		interiorMappingPSODesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
		interiorMappingPSODesc.pRootSignature = interiorMappingRootSig.Get();
		interiorMappingPSODesc.VS = CD3DX12_SHADER_BYTECODE(interiorMappingVS.Get());
		interiorMappingPSODesc.PS = CD3DX12_SHADER_BYTECODE(interiorMappingPS.Get());
		interiorMappingPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		interiorMappingPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
		interiorMappingPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
		interiorMappingPSODesc.SampleMask = UINT_MAX;
		interiorMappingPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		interiorMappingPSODesc.NumRenderTargets = 1;
		interiorMappingPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		interiorMappingPSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		interiorMappingPSODesc.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&interiorMappingPSODesc, IID_PPV_ARGS(interiorMappingPSO.GetAddressOf())));

	}

	//Light culling setup
	{

		//Creating the light culling root signature and pipeline state
		CD3DX12_DESCRIPTOR_RANGE1 computeRootRanges[1];
		CD3DX12_ROOT_PARAMETER1 lightCullingRootParams[LightCullingNumParameters];
		computeRootRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		lightCullingRootParams[LightCullingRootIndices::LightListSRV].InitAsShaderResourceView(0, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
		lightCullingRootParams[LightCullingRootIndices::DepthMapSRV].InitAsDescriptorTable(1, &computeRootRanges[0]);
		lightCullingRootParams[LightCullingRootIndices::VisibleLightIndicesUAV].InitAsUnorderedAccessView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);
		lightCullingRootParams[LightCullingRootIndices::LightCullingExternalDataCBV].InitAsConstantBufferView(0, 0);


		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1(_countof(lightCullingRootParams), lightCullingRootParams);

		ComPtr<ID3DBlob> computeSignature;
		ComPtr<ID3DBlob> computeError;

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
		ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)));

		ComPtr<ID3DBlob> lightCullingCS;

		ThrowIfFailed(D3DReadFileToBlob(L"LightCullingCS.cso", lightCullingCS.GetAddressOf()));

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
		computePSODesc.pRootSignature = computeRootSignature.Get();
		computePSODesc.CS = CD3DX12_SHADER_BYTECODE(lightCullingCS.Get());

		ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(computePipelineState.GetAddressOf())));
	}

	//vmf solver set up
	{
		CD3DX12_ROOT_PARAMETER1 rootParams[VMFFilterRootIndices::VMFFilterNumParameters + 1];
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParams[VMFFilterRootIndices::OutputRoughnessVMFUAV].InitAsDescriptorTable(1, &ranges[1]);
		rootParams[VMFFilterRootIndices::NormalRoughnessSRV].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[VMFFilterRootIndices::VMFFilterExternDataCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);
		rootParams[VMFFilterNumParameters].InitAsConstants(1, 1, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams);

		ComPtr<ID3DBlob> computeSignature;
		ComPtr<ID3DBlob> computeError;

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
		ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&vmfSofverRootSignature)));

		ComPtr<ID3DBlob> vmfSolverBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"VMFSolverCS.cso", vmfSolverBlob.GetAddressOf()));

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
		computePSODesc.pRootSignature = vmfSofverRootSignature.Get();
		computePSODesc.CS = CD3DX12_SHADER_BYTECODE(vmfSolverBlob.Get());

		ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(vmfSolverPSO.GetAddressOf())));

	}

	//velocity setup
	{
		CD3DX12_ROOT_PARAMETER1 velocityRootParams[2];
		velocityRootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		velocityRootParams[1].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);


		ComPtr<ID3DBlob> velocitySignature;
		ComPtr<ID3DBlob> velocityError;

		rootSignatureDesc.Init_1_1(_countof(velocityRootParams), velocityRootParams,
			0, nullptr, rootSignatureFlags);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
			velocitySignature.GetAddressOf(), velocityError.GetAddressOf()));

		ThrowIfFailed(device->CreateRootSignature(0, velocitySignature->GetBufferPointer(), velocitySignature->GetBufferSize(),
			IID_PPV_ARGS(velRootSig.GetAddressOf())));

		ComPtr<ID3DBlob> velocityWriteVS;
		ComPtr<ID3DBlob> velocityWritePS;

		ThrowIfFailed(D3DReadFileToBlob(L"VelocityBufferPS.cso", velocityWritePS.GetAddressOf()));
		ThrowIfFailed(D3DReadFileToBlob(L"VelocityBufferVS.cso", velocityWriteVS.GetAddressOf()));

		//creating a pipeline state object
		D3D12_GRAPHICS_PIPELINE_STATE_DESC velocityDesc = {};
		velocityDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
		velocityDesc.pRootSignature = velRootSig.Get();
		velocityDesc.VS = CD3DX12_SHADER_BYTECODE(velocityWriteVS.Get());
		velocityDesc.PS = CD3DX12_SHADER_BYTECODE(velocityWritePS.Get());
		velocityDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		velocityDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
		velocityDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
		velocityDesc.SampleMask = UINT_MAX;
		velocityDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		velocityDesc.NumRenderTargets = 1;
		velocityDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		velocityDesc.RTVFormats[0] = DXGI_FORMAT_R32G32_FLOAT;
		velocityDesc.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&velocityDesc, IID_PPV_ARGS(velPSO.GetAddressOf())));
	}

	//setting up post processing shaders
	{
		//Tone mapping
		{
			CD3DX12_DESCRIPTOR_RANGE1 toneMappingDescriptorRange[1];
			CD3DX12_ROOT_PARAMETER1 tonemappingRootParams[2];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			toneMappingDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			tonemappingRootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
			tonemappingRootParams[1].InitAsDescriptorTable(1, &toneMappingDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

			ComPtr<ID3DBlob> tonemappingSignature;
			ComPtr<ID3DBlob> tonemappingError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(tonemappingRootParams), tonemappingRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				tonemappingSignature.GetAddressOf(), tonemappingError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, tonemappingSignature->GetBufferPointer(), tonemappingSignature->GetBufferSize(),
				IID_PPV_ARGS(toneMappingRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> tonemappingPS;

			ThrowIfFailed(D3DReadFileToBlob(L"TonemappingPS.cso", tonemappingPS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a tonemapping pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC toneMappingPSODesc = {};
			toneMappingPSODesc.InputLayout = { };
			toneMappingPSODesc.pRootSignature = toneMappingRootSig.Get();
			toneMappingPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			toneMappingPSODesc.PS = CD3DX12_SHADER_BYTECODE(tonemappingPS.Get());
			toneMappingPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			toneMappingPSODesc.DepthStencilState.DepthEnable = false;
			toneMappingPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			toneMappingPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			toneMappingPSODesc.SampleMask = UINT_MAX;
			toneMappingPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			toneMappingPSODesc.NumRenderTargets = 1;
			toneMappingPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			toneMappingPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			toneMappingPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&toneMappingPSODesc, IID_PPV_ARGS(toneMappingPSO.GetAddressOf())));
		}

		//TAA
		{
			CD3DX12_DESCRIPTOR_RANGE1 taaDescriptorRange[4];
			CD3DX12_ROOT_PARAMETER1 taaRootParams[6];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			taaDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
			taaDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
			taaDescriptorRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
			taaDescriptorRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			taaRootParams[0].InitAsDescriptorTable(1, &taaDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
			taaRootParams[1].InitAsDescriptorTable(1, &taaDescriptorRange[1], D3D12_SHADER_VISIBILITY_PIXEL);
			taaRootParams[2].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			taaRootParams[3].InitAsDescriptorTable(1, &taaDescriptorRange[2], D3D12_SHADER_VISIBILITY_PIXEL);
			taaRootParams[4].InitAsDescriptorTable(1, &taaDescriptorRange[3], D3D12_SHADER_VISIBILITY_PIXEL);
			taaRootParams[5].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

			ComPtr<ID3DBlob> taaSignature;
			ComPtr<ID3DBlob> taaError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
			staticSamplers[1].Init(1, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(taaRootParams), taaRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				taaSignature.GetAddressOf(), taaError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, taaSignature->GetBufferPointer(), taaSignature->GetBufferSize(),
				IID_PPV_ARGS(taaRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> taaPS;

			ThrowIfFailed(D3DReadFileToBlob(L"TemporalAntiAliasingPS.cso", taaPS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a TAA pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC taaPSODesc = {};
			taaPSODesc.InputLayout = { };
			taaPSODesc.pRootSignature = taaRootSig.Get();
			taaPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			taaPSODesc.PS = CD3DX12_SHADER_BYTECODE(taaPS.Get());
			taaPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			taaPSODesc.DepthStencilState.DepthEnable = false;
			taaPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			taaPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			taaPSODesc.SampleMask = UINT_MAX;
			taaPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			taaPSODesc.NumRenderTargets = 1;
			taaPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			taaPSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			taaPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&taaPSODesc, IID_PPV_ARGS(taaPSO.GetAddressOf())));
		}

		//Sharpness
		{
			CD3DX12_DESCRIPTOR_RANGE1 sharpenDescriptorRange[1];
			CD3DX12_ROOT_PARAMETER1 sharpenRootParams[1];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			sharpenDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			sharpenRootParams[0].InitAsDescriptorTable(1, &sharpenDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);


			ComPtr<ID3DBlob> sharpenSignature;
			ComPtr<ID3DBlob> sharpenError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(sharpenRootParams), sharpenRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				sharpenSignature.GetAddressOf(), sharpenError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, sharpenSignature->GetBufferPointer(), sharpenSignature->GetBufferSize(),
				IID_PPV_ARGS(sharpenRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> sharpenPS;

			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenSharpenPS.cso", sharpenPS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a sharpen pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC sharpenPSODesc = {};
			sharpenPSODesc.InputLayout = { };
			sharpenPSODesc.pRootSignature = sharpenRootSig.Get();
			sharpenPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			sharpenPSODesc.PS = CD3DX12_SHADER_BYTECODE(sharpenPS.Get());
			sharpenPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			sharpenPSODesc.DepthStencilState.DepthEnable = false;
			sharpenPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			sharpenPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			sharpenPSODesc.SampleMask = UINT_MAX;
			sharpenPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			sharpenPSODesc.NumRenderTargets = 1;
			sharpenPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			sharpenPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			sharpenPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&sharpenPSODesc, IID_PPV_ARGS(sharpenPSO.GetAddressOf())));
		}

		//FXAA
		{
			CD3DX12_DESCRIPTOR_RANGE1 fxaaDescriptorRange[1];
			CD3DX12_ROOT_PARAMETER1 fxaaRootParams[1];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			fxaaDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			fxaaRootParams[0].InitAsDescriptorTable(1, &fxaaDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);


			ComPtr<ID3DBlob> fxaaSignature;
			ComPtr<ID3DBlob> fxaaError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(fxaaRootParams), fxaaRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				fxaaSignature.GetAddressOf(), fxaaError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, fxaaSignature->GetBufferPointer(), fxaaSignature->GetBufferSize(),
				IID_PPV_ARGS(fxaaRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> fxaaPS;

			ThrowIfFailed(D3DReadFileToBlob(L"FxaaPS.cso", fxaaPS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a fxaa pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC fxaaPSODesc = {};
			fxaaPSODesc.InputLayout = { };
			fxaaPSODesc.pRootSignature = fxaaRootSig.Get();
			fxaaPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			fxaaPSODesc.PS = CD3DX12_SHADER_BYTECODE(fxaaPS.Get());
			fxaaPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			fxaaPSODesc.DepthStencilState.DepthEnable = false;
			fxaaPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			fxaaPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			fxaaPSODesc.SampleMask = UINT_MAX;
			fxaaPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			fxaaPSODesc.NumRenderTargets = 1;
			fxaaPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			fxaaPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			fxaaPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&fxaaPSODesc, IID_PPV_ARGS(fxaaPSO.GetAddressOf())));
		}

		//fullscreen pass through
		{
			CD3DX12_DESCRIPTOR_RANGE1 passthroughDescriptorRange[1];
			CD3DX12_ROOT_PARAMETER1 passthroughRootParams[1];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			passthroughDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			passthroughRootParams[0].InitAsDescriptorTable(1, &passthroughDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);


			ComPtr<ID3DBlob> passthroughSignature;
			ComPtr<ID3DBlob> passthroughError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(passthroughRootParams), passthroughRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				passthroughSignature.GetAddressOf(), passthroughError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, passthroughSignature->GetBufferPointer(), passthroughSignature->GetBufferSize(),
				IID_PPV_ARGS(passthroughRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> passthroughPS;

			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenPassThroughPS.cso", passthroughPS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a passthrough pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC passthroughPSODesc = {};
			passthroughPSODesc.InputLayout = { };
			passthroughPSODesc.pRootSignature = passthroughRootSig.Get();
			passthroughPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			passthroughPSODesc.PS = CD3DX12_SHADER_BYTECODE(passthroughPS.Get());
			passthroughPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			passthroughPSODesc.DepthStencilState.DepthEnable = false;
			passthroughPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			passthroughPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			passthroughPSODesc.SampleMask = UINT_MAX;
			passthroughPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			passthroughPSODesc.NumRenderTargets = 1;
			passthroughPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			passthroughPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			passthroughPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&passthroughPSODesc, IID_PPV_ARGS(passthroughPSO.GetAddressOf())));
		}

		{
			CD3DX12_DESCRIPTOR_RANGE1 descriptorRange[2];
			CD3DX12_ROOT_PARAMETER1 rootParams[3];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
			descriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			rootParams[0].InitAsConstantBufferView(0, 0);
			rootParams[1].InitAsDescriptorTable(1, &descriptorRange[0]);
			rootParams[2].InitAsDescriptorTable(1, &descriptorRange[1]);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				signature.GetAddressOf(), error.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
				IID_PPV_ARGS(fsrRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> shaderRCAS;
			ComPtr<ID3DBlob> shaderEASU;

			ThrowIfFailed(D3DReadFileToBlob(L"FSR_RCASPass.cso", shaderRCAS.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FSR_EASUPass.cso", shaderEASU.GetAddressOf()));

			//creating a passthrough pipeline state
			D3D12_COMPUTE_PIPELINE_STATE_DESC rcasPsoDesc = {};
			rcasPsoDesc.pRootSignature = fsrRootSig.Get();
			rcasPsoDesc.CS = CD3DX12_SHADER_BYTECODE(shaderRCAS.Get());
			
			ThrowIfFailed(device->CreateComputePipelineState(&rcasPsoDesc, IID_PPV_ARGS(fsrRCASPso.GetAddressOf())));

			//creating a passthrough pipeline state
			D3D12_COMPUTE_PIPELINE_STATE_DESC easuPsoDesc = {};
			easuPsoDesc.pRootSignature = fsrRootSig.Get();
			easuPsoDesc.CS = CD3DX12_SHADER_BYTECODE(shaderEASU.Get());

			ThrowIfFailed(device->CreateComputePipelineState(&easuPsoDesc, IID_PPV_ARGS(fsrEASUPso.GetAddressOf())));
		}

		{
			CD3DX12_DESCRIPTOR_RANGE1 descriptorRange[5];
			CD3DX12_ROOT_PARAMETER1 rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_NumIndices];

			descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
			descriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0);
			descriptorRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0);
			descriptorRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0);
			descriptorRange[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4, 0);

			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferPos].InitAsDescriptorTable(1, &descriptorRange[0]);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferNorm].InitAsDescriptorTable(1, &descriptorRange[1]);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferDif].InitAsDescriptorTable(1, &descriptorRange[2]);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferRoughMetal].InitAsDescriptorTable(1, &descriptorRange[3]);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_OutColor].InitAsDescriptorTable(1, &descriptorRange[4]);

			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_Reservoirs].InitAsUnorderedAccessView(5, 0);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_SampleSequences].InitAsUnorderedAccessView(6, 0);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_OutReservoirs].InitAsUnorderedAccessView(7, 0);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_Lights].InitAsShaderResourceView(0, 2);
			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_AccelStruct].InitAsShaderResourceView(0, 0);

			rootParams[RestrirSpatialReuseIndices::RestirSpatialReuse_ExternData].InitAsConstants(3, 0, 0);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
				0, nullptr, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				signature.GetAddressOf(), error.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
				IID_PPV_ARGS(restirSpatialReuseRootSig.GetAddressOf())));

			{
				ComPtr<ID3DBlob> shader;

				ThrowIfFailed(D3DReadFileToBlob(L"ReStirSpatialReuseAndFinalShade.cso", shader.GetAddressOf()));

				//creating a passthrough pipeline state
				D3D12_COMPUTE_PIPELINE_STATE_DESC rcasPsoDesc = {};
				rcasPsoDesc.pRootSignature = restirSpatialReuseRootSig.Get();
				rcasPsoDesc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());

				ThrowIfFailed(device->CreateComputePipelineState(&rcasPsoDesc, IID_PPV_ARGS(restirSpatialReusePSO.GetAddressOf())));
			}

			{
				ComPtr<ID3DBlob> shader;

				ThrowIfFailed(D3DReadFileToBlob(L"ReStirGISpatialReuseAndFinalShade.cso", shader.GetAddressOf()));

				//creating a passthrough pipeline state
				D3D12_COMPUTE_PIPELINE_STATE_DESC rcasPsoDesc = {};
				rcasPsoDesc.pRootSignature = restirSpatialReuseRootSig.Get();
				rcasPsoDesc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());

				ThrowIfFailed(device->CreateComputePipelineState(&rcasPsoDesc, IID_PPV_ARGS(restirGISpatialReusePSO.GetAddressOf())));
			}
		}

		//RT combine
		{
			CD3DX12_DESCRIPTOR_RANGE1 rtCombineDescriptorRange[4];
			CD3DX12_ROOT_PARAMETER1 rtCombineRootParams[4];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			rtCombineDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
			rtCombineDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
			rtCombineDescriptorRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
			rtCombineDescriptorRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			rtCombineRootParams[0].InitAsDescriptorTable(1, &rtCombineDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
			rtCombineRootParams[1].InitAsDescriptorTable(1, &rtCombineDescriptorRange[1], D3D12_SHADER_VISIBILITY_PIXEL);
			rtCombineRootParams[2].InitAsDescriptorTable(1, &rtCombineDescriptorRange[2], D3D12_SHADER_VISIBILITY_PIXEL);
			rtCombineRootParams[3].InitAsDescriptorTable(1, &rtCombineDescriptorRange[3], D3D12_SHADER_VISIBILITY_PIXEL);


			ComPtr<ID3DBlob> rtCombineSignature;
			ComPtr<ID3DBlob> rtCombineError;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			rootSignatureDesc.Init_1_1(_countof(rtCombineRootParams), rtCombineRootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				rtCombineSignature.GetAddressOf(), rtCombineError.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, rtCombineSignature->GetBufferPointer(), rtCombineSignature->GetBufferSize(),
				IID_PPV_ARGS(rtCombineRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> rtCombine;

			ThrowIfFailed(D3DReadFileToBlob(L"RaytraceFinalCombine.cso", rtCombine.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a passthrough pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC rtCombinePSODesc = {};
			rtCombinePSODesc.InputLayout = { };
			rtCombinePSODesc.pRootSignature = rtCombineRootSig.Get();
			rtCombinePSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			rtCombinePSODesc.PS = CD3DX12_SHADER_BYTECODE(rtCombine.Get());
			rtCombinePSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			rtCombinePSODesc.DepthStencilState.DepthEnable = false;
			rtCombinePSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			rtCombinePSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			rtCombinePSODesc.SampleMask = UINT_MAX;
			rtCombinePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			rtCombinePSODesc.NumRenderTargets = 1;
			rtCombinePSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			rtCombinePSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			rtCombinePSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&rtCombinePSODesc, IID_PPV_ARGS(rtCombinePSO.GetAddressOf())));
		}


		//RT combine
		{
			CD3DX12_DESCRIPTOR_RANGE1 desciptorRanges[2];
			CD3DX12_ROOT_PARAMETER1 rootParams[BilateralBlur::BilateralBlurNumParams];

			//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
			desciptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
			desciptorRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

			//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
			rootParams[BilateralBlur::MainColorTex].InitAsDescriptorTable(1, &desciptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
			rootParams[BilateralBlur::DepthTexBlur].InitAsDescriptorTable(1, &desciptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
			rootParams[BilateralBlur::BilateralBlurExternalData].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);


			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT);

			rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
				_countof(staticSamplers), staticSamplers, rootSignatureFlags);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
				signature.GetAddressOf(), error.GetAddressOf()));

			ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
				IID_PPV_ARGS(bilateralRootSig.GetAddressOf())));

			ComPtr<ID3DBlob> fullcreenVS;
			ComPtr<ID3DBlob> bilateral;

			ThrowIfFailed(D3DReadFileToBlob(L"BilateralBlur.cso", bilateral.GetAddressOf()));
			ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

			//creating a passthrough pipeline state
			D3D12_GRAPHICS_PIPELINE_STATE_DESC bilateralPSODesc = {};
			bilateralPSODesc.InputLayout = { };
			bilateralPSODesc.pRootSignature = bilateralRootSig.Get();
			bilateralPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
			bilateralPSODesc.PS = CD3DX12_SHADER_BYTECODE(bilateral.Get());
			bilateralPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			bilateralPSODesc.DepthStencilState.DepthEnable = false;
			bilateralPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
			bilateralPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
			bilateralPSODesc.SampleMask = UINT_MAX;
			bilateralPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			bilateralPSODesc.NumRenderTargets = 1;
			bilateralPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			bilateralPSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			bilateralPSODesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->CreateGraphicsPipelineState(&bilateralPSODesc, IID_PPV_ARGS(bilateralPSO.GetAddressOf())));
		}

		//blue noise permutation pass
		{

			CD3DX12_DESCRIPTOR_RANGE1 rootRanges[3];
			CD3DX12_ROOT_PARAMETER1 rootParams[BlueNoiseDithering::BNDSNumParams];

			rootRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			rootRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			rootRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

			rootParams[BlueNoiseDithering::BlueNoiseTex].InitAsDescriptorTable(1, &rootRanges[0]);
			rootParams[BlueNoiseDithering::PrevFrameNoisy].InitAsDescriptorTable(1, &rootRanges[1]);
			rootParams[BlueNoiseDithering::RetargetTex].InitAsDescriptorTable(1, &rootRanges[2]);
			rootParams[BlueNoiseDithering::NewSequences].InitAsUnorderedAccessView(0, 0);
			rootParams[BlueNoiseDithering::RetargettedSequencesBNDS].InitAsUnorderedAccessView(1, 0);
			rootParams[BlueNoiseDithering::FrameNum].InitAsConstantBufferView(0,1);

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);


			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers);

			ComPtr<ID3DBlob> computeSignature;
			ComPtr<ID3DBlob> computeError;

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
			ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&bndsComputeRootSignature)));

			ComPtr<ID3DBlob> computeShader;

			ThrowIfFailed(D3DReadFileToBlob(L"ComputeBlueNoisePermutedSequencesCS.cso", computeShader.GetAddressOf()));

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
			computePSODesc.pRootSignature = bndsComputeRootSignature.Get();
			computePSODesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

			ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(bndsPipelineState.GetAddressOf())));
		}

		//blue noise retargeting pass
		{

			CD3DX12_DESCRIPTOR_RANGE1 rootRanges[1];
			CD3DX12_ROOT_PARAMETER1 rootParams[RetargetingPass::RetargetingPassNumParams];

			rootRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

			rootParams[RetargetingPass::RetargetTexture].InitAsDescriptorTable(1, &rootRanges[0]);
			rootParams[RetargetingPass::RetargetedSequences].InitAsUnorderedAccessView(0, 0);
			rootParams[RetargetingPass::OldSequences].InitAsUnorderedAccessView(1, 0);
			rootParams[RetargetingPass::RetargetingPassCBV].InitAsConstantBufferView(0, 0);

			CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
			staticSamplers[0].Init(0, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT);


			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers);

			ComPtr<ID3DBlob> computeSignature;
			ComPtr<ID3DBlob> computeError;

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
			ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&retargetingRootSignature)));

			ComPtr<ID3DBlob> computeShader;

			ThrowIfFailed(D3DReadFileToBlob(L"RetargetingPass.cso", computeShader.GetAddressOf()));

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
			computePSODesc.pRootSignature = retargetingRootSignature.Get();
			computePSODesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

			ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(retargetingPipelineState.GetAddressOf())));
		}
	}

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
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer) * 3);
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(cbufferUploadHeap.GetAddressOf())
	));

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightConstantBufferResource.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightingConstantBufferResource.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightCullingCBVResource.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(bndsCBResource.GetAddressOf())
	));


	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(bmfrPreProcessCBV.GetAddressOf())
	));

	ZeroMemory(&lightData, sizeof(lightData));
	ZeroMemory(&lightingData, sizeof(lightingData));

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Light) * MAX_LIGHTS);
	//creating the light list srv
	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().uploadHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(lightListResource.GetAddressOf())
	));

	int workGroupsX = (renderWidth + (renderWidth % TILE_SIZE)) / TILE_SIZE;
	int workGroupsY = (renderHeight + (renderHeight % TILE_SIZE)) / TILE_SIZE;
	size_t numberOfTiles = workGroupsX * workGroupsY;

	visibleLightIndices = new UINT[workGroupsX * workGroupsY * 1024];

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * 1024 * numberOfTiles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(visibleLightIndicesBuffer.resource.GetAddressOf())
	));

	visibleLightIndicesBuffer.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(renderWidth*renderHeight*4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(sampleSequences.resource.GetAddressOf())
	));

	sampleSequences.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(retargetedSequences.resource.GetAddressOf())
	));

	retargetedSequences.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(AlignUp(renderWidth * renderHeight * sizeof(Reservoir), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(currentReservoir.resource.GetAddressOf())
	));

	currentReservoir.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(intermediateReservoir.resource.GetAddressOf())
	));

	intermediateReservoir.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(AlignUp(renderWidth * renderHeight * sizeof(GIReservoir), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(indirectDiffuseTemporalReservoir.resource.GetAddressOf())
	));

	indirectDiffuseTemporalReservoir.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	ThrowIfFailed(device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(indirectDiffuseSpatialReservoir.resource.GetAddressOf())
	));

	indirectDiffuseSpatialReservoir.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	bndsCBResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&bndsDataBegin));

	lights = new Light[MAX_LIGHTS];

	ZeroMemory(lights, MAX_LIGHTS * sizeof(Light));

	//lights[lightCount].type = LIGHT_TYPE_DIR;
	//lights[lightCount].direction = XMFLOAT3(1, -1, 0);
	//lights[lightCount].color = XMFLOAT3(1, 1, 1);
	//lights[lightCount].intensity = 1;
	//lightCount++;
	
	//lights[lightCount].type = LIGHT_TYPE_AREA_RECT;
	//lights[lightCount].color = XMFLOAT3(1, 1, 1);
	//lights[lightCount].rectLight.height = 3;
	//lights[lightCount].rectLight.width = 3;
	//lights[lightCount].rectLight.rotY = 0;
	//lights[lightCount].rectLight.rotZ = 0;
	//lights[lightCount].rectLight.rotX = 0;
	//lights[lightCount].position = XMFLOAT3(-6, -2, 0);
	//lights[lightCount].intensity = 1;
	//lightCount++;
	
	//lights[lightCount].type = LIGHT_TYPE_AREA_DISK;
	//lights[lightCount].color = XMFLOAT3(1, 1, 1);
	//lights[lightCount].rectLight.height = 2;
	//lights[lightCount].rectLight.width = 2;
	//lights[lightCount].rectLight.rotY = 0;
	//lights[lightCount].rectLight.rotZ = 0;
	//lights[lightCount].rectLight.rotX = 0;
	//lights[lightCount].position = XMFLOAT3(-2, -2, 0);
	//lights[lightCount].intensity = 10;
	//lightCount++;
	
	//lights[lightCount].type = LIGHT_TYPE_POINT;
	//lights[lightCount].color = XMFLOAT3(1, 0, 0);
	//lights[lightCount].range = 20;
	//lights[lightCount].position = XMFLOAT3(3, 0, 0);
	//lights[lightCount].intensity = 1;
	//lightCount++;

	lights[lightCount].type = LIGHT_TYPE_POINT;
	lights[lightCount].color = XMFLOAT3(1, 0, 0);
	lights[lightCount].range = 50;
	lights[lightCount].position = XMFLOAT3(30, 30, 0);
	lights[lightCount].intensity = 3;
	lightCount++;

	lights[lightCount].type = LIGHT_TYPE_POINT;
	lights[lightCount].color = XMFLOAT3(0, 1, 0);
	lights[lightCount].range = 50;
	lights[lightCount].position = XMFLOAT3(-30, 30, 0);
	lights[lightCount].intensity = 3;
	lightCount++;

	lights[lightCount].type = LIGHT_TYPE_POINT;
	lights[lightCount].color = XMFLOAT3(1, 0, 0);
	lights[lightCount].range = 50;
	lights[lightCount].position = XMFLOAT3(0, 30, -30);
	lights[lightCount].intensity = 3;
	lightCount++;
	
	for (int i = 0; i < 100; i++)
	{
		lights[lightCount].type = LIGHT_TYPE_POINT;
		lights[lightCount].color = GetRandomFloat3(0, 1);
		lights[lightCount].range = 50;
		lights[lightCount].position = GetRandomFloat3(-50, 50);
		lights[lightCount].intensity = 1;
		lightCount++;
	}

	lightConstantBufferResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&lightCbufferBegin));
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	lightingData.cameraPosition = mainCamera->GetPosition();
	lightingData.lightCount = lightCount;
	lightingData.cameraForward = mainCamera->GetDirection();

	lightingConstantBufferResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&lightingCbufferBegin));
	memcpy(lightingCbufferBegin, &lightingData, sizeof(lightingData));

	lightCullingCBVResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&lightCullingExternBegin));


	lightListResource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&lightBufferBegin));
	memcpy(lightBufferBegin, lights, MAX_LIGHTS * sizeof(Light));

	bmfrPreProcessCBV->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&bmfrPreprocessBegin));

	UINT64 cbufferOffset = 0;
	mesh1 = std::make_shared<Mesh>("../../Assets/Models/sphere.obj");
	mesh2 = std::make_shared<Mesh>("../../Assets/Models/cube.obj");
	mesh3 = std::make_shared<Mesh>("../../Assets/Models/Cerebrus.obj");
	sharkMesh = std::make_shared<Mesh>("../../Assets/Models/bird2.obj");
	faceMesh = std::make_shared<Mesh>("../../Assets/Models/face.obj");
	skyDome = std::make_shared<Mesh>("../../Assets/Models/sky_dome.obj");
	rect = std::make_shared<Mesh>("../../Assets/Models/RectLight.obj");
	disk = std::make_shared<Mesh>("../../Assets/Models/disk.obj");



	//creating the vertex buffer
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	float aspectRatio = static_cast<float>(renderWidth / renderHeight);

	//CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mainCPUDescriptorHandle, 0, cbvDescriptorSize);
	material1 = std::make_shared<Material>(
		L"../../Assets/Textures/GoldDiffuse.png", L"../../Assets/Textures/GoldNormal.png",
		L"../../Assets/Textures/GoldRoughness.png", L"../../Assets/Textures/GoldMetallic.png");
	material2 = std::make_shared<Material>(
		L"../../Assets/Textures/TransparentTest.png", L"../../Assets/Textures/DefaultNormal.jpg",
		L"../../Assets/Textures/LayeredRoughness.png", L"../../Assets/Textures/LayeredMetallic.png");

	std::shared_ptr<Material> material3 = std::make_shared<Material>(
		L"../../Assets/Textures/CerebrusDiffuse.jpg", L"../../Assets/Textures/CerebrusNormal.jpg",
		L"../../Assets/Textures/CerebrusRoughness.jpg", L"../../Assets/Textures/CerebrusMetalness.jpg");

	std::shared_ptr<Material> material4 = std::make_shared<Material>(
		L"../../Assets/Textures/Head_Diffuse.png", L"../../Assets/Textures/Head_Normal.png");

	std::shared_ptr<Material> material5 = std::make_shared<Material>(
		L"../../Assets/Textures/GoldMetallic.png", L"../../Assets/Textures/GoldNormal.png",
		L"../../Assets/Textures/DefaultRoughness.png",
		L"../../Assets/Textures/LayeredMetallic.png");

	interiorMaterial = std::make_shared<InteriorMaterial>(5);

	materials.emplace_back(material1);
	materials.emplace_back(material2);
	materials.emplace_back(material3);
	materials.emplace_back(material4);
	materials.emplace_back(material5);

	//allocate volumes and skyboxes here
	for (size_t i = 0; i < materials.size(); i++)
	{
		gpuHeapRingBuffer->AllocateStaticDescriptors(4, materials[i]->GetDescriptorHeap());
		materials[i]->materialIndex = gpuHeapRingBuffer->GetNumStaticResources()-4;
	}

	gpuHeapRingBuffer->AllocateStaticDescriptors(4, interiorMaterial->GetDescriptorHeap());
	interiorMaterial->materialIndex = gpuHeapRingBuffer->GetNumStaticResources() - 4;

	auto numStaticResources = gpuHeapRingBuffer->GetNumStaticResources();
	//for (int i = 0; i < materials.size(); i++)
	//{
	//	materials[i]->GenerateMaps(vmfSolverPSO, vmfSofverRootSignature, gpuHeapRingBuffer);
	//	materials[i]->prefilteredMapIndex = gpuHeapRingBuffer->GetNumStaticResources() - 2;
	//
	//
	//}


	computeCommandList->Close();
	ID3D12CommandList* computeCommandLists[] = { computeCommandList.Get() };
	computeCommandQueue->ExecuteCommandLists(_countof(computeCommandLists), computeCommandLists);
	ThrowIfFailed(computeCommandList->Reset(computeCommandAllocator[frameIndex].Get(), computePipelineState.Get()));

	ThrowIfFailed(computeCommandQueue->Signal(computeFence.Get(), fenceValues[frameIndex]));
	ThrowIfFailed(commandQueue->Wait(computeFence.Get(), fenceValues[frameIndex]));

	commandList->Close();
	ID3D12CommandList* pcommandLists[] = { commandList.Get() };
	D3DX12Residency::ResidencySet* ppSets[] = { residencySet.get() };
	commandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);


	WaitForPreviousFrame();

	ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

	{
		commandList->Close();
		ID3D12CommandList* pcommandLists[] = { commandList.Get() };
		D3DX12Residency::ResidencySet* ppSets[] = { residencySet.get() };
		commandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);

		WaitForPreviousFrame();

		ThrowIfFailed(
			commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));
	}

	entity1 = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, "entity1");
	entity1->AddModel("../../Assets/Models/cube.obj");
	entity1->AddMaterial(material1->materialIndex);

	entity2 = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, "entity2");
	entity2->AddModel("../../Assets/Models/cube.obj");
	entity2->AddMaterial(material2->materialIndex);

	entity3 = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, "entity3");
	entity3->AddModel("../../Assets/Models/cube.obj");
	entity3->AddMaterial(material2->materialIndex);

	entity4 = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, "entity4");
	entity4->AddModel("../../Assets/Models/cube.obj");
	entity4->AddMaterial(material1->materialIndex);

	entity6 = std::make_shared<Entity>(registry, interiorMappingPSO, interiorMappingRootSig, "entity5");
	entity6->AddModel("../../Assets/Models/cube.obj");
	//entity6->AddMaterial(interiorMaterial->materialIndex);

	diskEntity = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, "entity6");
	diskEntity->AddModel("../../Assets/Models/disk.obj");
	diskEntity->AddMaterial(material2->materialIndex);
	diskEntity->SetTag("diskEntity");

	cerebrus = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature);
	cerebrus->AddModel("../../Assets/Models/Cerebrus.obj");
	cerebrus->AddMaterial(material3->materialIndex);
	cerebrus->SetTag("cerebrus");

	entity1->SetPosition(XMFLOAT3(0, -10, 1.5f));
	entity1->SetScale(XMFLOAT3(1000, 10, 1000));
	entity2->SetPosition(XMFLOAT3(1, 0, 1.0f));
	entity3->SetPosition(XMFLOAT3(-3, 0, 1.f));
	entity4->SetPosition(XMFLOAT3(8, -8, 1.f));
	entity6->SetPosition(XMFLOAT3(-10, 0, 0));
	entity6->SetScale(XMFLOAT3(3, 3, 3));
	diskEntity->SetPosition(XMFLOAT3(-1, 0, 0));
	cerebrus->SetScale(XMFLOAT3(0.3, 0.3, 0.3));
	cerebrus->SetPosition(XMFLOAT3(0, 3, 2));

	auto initialRot = entity6->GetRotation();
	XMVECTOR finalRot = XMQuaternionRotationRollPitchYaw(0, 0, -90 * 3.14159f / 180);
	XMStoreFloat4(&initialRot, finalRot);
	entity6->SetRotation(initialRot);

	entity1->PrepareConstantBuffers(residencyManager, residencySet);
	entity2->PrepareConstantBuffers(residencyManager, residencySet);
	entity3->PrepareConstantBuffers(residencyManager, residencySet);
	entity4->PrepareConstantBuffers(residencyManager, residencySet);
	entity6->PrepareConstantBuffers(residencyManager, residencySet);
	diskEntity->PrepareConstantBuffers(residencyManager, residencySet);
	cerebrus->PrepareConstantBuffers(residencyManager, residencySet);


	entities.emplace_back(entity1);
	entities.emplace_back(entity2);
	entities.emplace_back(entity3);
	entities.emplace_back(entity4);
	entities.emplace_back(cerebrus);

	for (int i = 0; i < 10; i++)
	{
		flockers.emplace_back(std::make_shared<Entity>(registry, pipelineState, rootSignature, "flocker"+std::to_string(i)));
		flockers[i]->PrepareConstantBuffers(residencyManager, residencySet);
		const auto enttID = flockers[i]->GetEntityID();

		flockers[i]->SetPosition(XMFLOAT3(static_cast<float>(i + 6), static_cast<float>(i - 6), 0.f));
		//registry.assign<Flocker>(enttID, XMFLOAT3(i+2,i-2,0), 2 ,XMFLOAT3(0,0,0), 10,XMFLOAT3(0,0,0),1);
		auto valid = registry.valid(enttID);

		auto& flocker = registry.assign<Flocker>(enttID);
		flocker.pos = XMFLOAT3(static_cast<float>(i + 6), static_cast<float>(i - 6), 0.f);
		flocker.vel = XMFLOAT3(0, 0, 0);
		flocker.acceleration = XMFLOAT3(0, 0, 0);
		flocker.mass = 2;
		flocker.maxSpeed = 2;
		flocker.safeDistance = 1;
	}

	for (size_t i = 0; i < entities.size(); i++)
	{
		entityNames[i] = entities[i]->GetTag();
	}


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
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(skyboxPSO.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mainCPUDescriptorHandle, (INT)entities.size() + 1, cbvDescriptorSize);
	//creating the skybox
	skybox = std::make_shared<Skybox>(L"../../Assets/Textures/skybox5.hdr", mesh1,skyboxPSO, skyboxRootSignature, false);

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
	irradiancePsoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
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
	prefiltermapPSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
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
	//Direct Lighting
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
		sbtGenerator.AddRayGenerationProgram(L"RayGen", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(), (void*)retargetedSequences.resource->GetGPUVirtualAddress(),
			(void*)currentReservoir.resource->GetGPUVirtualAddress(), (void*)intermediateReservoir.resource->GetGPUVirtualAddress()});
		sbtGenerator.AddMissProgram(L"Miss", { heapPointer });

		for (size_t i = 0; i < entities.size(); i++)
		{
			auto meshes = entities[i]->GetModel()->GetMeshes();
			for (size_t j = 0; j < meshes.size(); j++)
			{
				UINT64 materialIndex = meshes[j]->GetMaterialID();
				auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
				sbtGenerator.AddHitGroup(L"HitGroup", { (void*)meshes[j]->GetVertexBufferResource()->GetGPUVirtualAddress(),
					(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
				sbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
			}
		}

		sbtGenerator.AddMissProgram(L"ShadowMiss", {});
		sbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

		//compute the size of the SBT
		UINT32 sbtSize = sbtGenerator.ComputeSBTSize()*2;

		//upload heap for the sbt
		sbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), AlignUp(sbtSize, 256u), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		sbtResource->SetName(L"SBT Resource");

		if (!sbtResource)
			throw std::logic_error("Could not create SBT resource");

		//compile the sbt from the above info
		sbtGenerator.Generate(sbtResource.Get(), rtStateObjectProps.Get());
	}

		//Direct Transparent Lighting
	{
		//shader binding tables define the raygen, miss, and hit group shaders
		//these resources are interpreted by the shader
		transparentSbtGenerator.Reset();

		//getting the descriptor heap handle
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = rtDescriptorHeap.GetHeap()->GetGPUDescriptorHandleForHeapStart();

		//reinterpreting the above pointer as a void pointer
		auto heapPointer = reinterpret_cast<UINT64*>(gpuHandle.ptr);

		//the ray generation shader needs external data therefore it needs the pointer to the heap
		//the miss and hit group shaders don't have any data
		transparentSbtGenerator.AddRayGenerationProgram(L"RayGenTransparency", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(), (void*)retargetedSequences.resource->GetGPUVirtualAddress(),
			(void*)currentReservoir.resource->GetGPUVirtualAddress(), (void*)intermediateReservoir.resource->GetGPUVirtualAddress()});
		transparentSbtGenerator.AddMissProgram(L"Miss", { heapPointer });

		for (size_t i = 0; i < entities.size(); i++)
		{
			auto meshes = entities[i]->GetModel()->GetMeshes();
			for (size_t j = 0; j < meshes.size(); j++)
			{
				UINT64 materialIndex = meshes[j]->GetMaterialID();
				auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
				transparentSbtGenerator.AddHitGroup(L"HitGroup", { (void*)meshes[j]->GetVertexBufferResource()->GetGPUVirtualAddress(),
					(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
				transparentSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
			}
		}

		transparentSbtGenerator.AddMissProgram(L"ShadowMiss", {});
		transparentSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

		//compute the size of the SBT
		UINT32 sbtSize = transparentSbtGenerator.ComputeSBTSize()*2;

		//upload heap for the sbt
		transparentSbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), AlignUp(sbtSize, 256u), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		transparentSbtResource->SetName(L"SBT Resource");

		if (!transparentSbtResource)
			throw std::logic_error("Could not create SBT resource");

		//compile the sbt from the above info
		transparentSbtGenerator.Generate(transparentSbtResource.Get(), rtTransparentStateObjectProps.Get());
	}

	//Indirect Diffuse
	{
		//shader binding tables define the raygen, miss, and hit group shaders
		//these resources are interpreted by the shader
		indirectDiffuseSbtGenerator.Reset();

		//getting the descriptor heap handle
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = rtDescriptorHeap.GetHeap()->GetGPUDescriptorHandleForHeapStart();

		//reinterpreting the above pointer as a void pointer
		auto heapPointer = reinterpret_cast<UINT64*>(gpuHandle.ptr);

		//the ray generation shader needs external data therefore it needs the pointer to the heap
		//the miss and hit group shaders don't have any data
		indirectDiffuseSbtGenerator.AddRayGenerationProgram(L"IndirectDiffuseRayGen", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress()
			, (void*)retargetedSequences.resource->GetGPUVirtualAddress(), (void*)indirectDiffuseTemporalReservoir.resource->GetGPUVirtualAddress(), (void*)indirectDiffuseSpatialReservoir.resource->GetGPUVirtualAddress() });
		indirectDiffuseSbtGenerator.AddMissProgram(L"Miss", { heapPointer });

		for (size_t i = 0; i < entities.size(); i++)
		{
			auto meshes = entities[i]->GetModel()->GetMeshes();
			for (size_t j = 0; j < meshes.size(); j++)
			{
				UINT64 materialIndex = meshes[j]->GetMaterialID();
				auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
				indirectDiffuseSbtGenerator.AddHitGroup(L"HitGroup", { (void*)meshes[j]->GetVertexBufferResource()->GetGPUVirtualAddress(),
					(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
				indirectDiffuseSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
			}
		}

		indirectDiffuseSbtGenerator.AddMissProgram(L"ShadowMiss", {});
		indirectDiffuseSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

		//compute the size of the SBT
		UINT32 sbtSize = indirectDiffuseSbtGenerator.ComputeSBTSize();

		//upload heap for the sbt
		indirectDiffuseSbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		if (!indirectDiffuseSbtResource)
			throw std::logic_error("Could not create SBT resource");

		//compile the sbt from the above info
		indirectDiffuseSbtGenerator.Generate(indirectDiffuseSbtResource.Get(), indirectDiffuseRtStateObjectProps.Get());
	}

	//Indirect Specular
	{
		//shader binding tables define the raygen, miss, and hit group shaders
		//these resources are interpreted by the shader
		indirectSpecularSbtGenerator.Reset();

		//getting the descriptor heap handle
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = rtDescriptorHeap.GetHeap()->GetGPUDescriptorHandleForHeapStart();

		//reinterpreting the above pointer as a void pointer
		auto heapPointer = reinterpret_cast<UINT64*>(gpuHandle.ptr);

		//the ray generation shader needs external data therefore it needs the pointer to the heap
		//the miss and hit group shaders don't have any data
		indirectSpecularSbtGenerator.AddRayGenerationProgram(L"IndirectSpecularRayGen", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress()
			, (void*)retargetedSequences.resource->GetGPUVirtualAddress() });
		indirectSpecularSbtGenerator.AddMissProgram(L"Miss", { heapPointer });

		for (size_t i = 0; i < entities.size(); i++)
		{
			auto meshes = entities[i]->GetModel()->GetMeshes();
			for (size_t j = 0; j < meshes.size(); j++)
			{
				UINT64 materialIndex = meshes[j]->GetMaterialID();
				auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
				indirectSpecularSbtGenerator.AddHitGroup(L"HitGroup", { (void*)meshes[j]->GetVertexBufferResource()->GetGPUVirtualAddress(),
					(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
				indirectSpecularSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
			}
		}

		indirectSpecularSbtGenerator.AddMissProgram(L"ShadowMiss", {});
		indirectSpecularSbtGenerator.AddHitGroup(L"ShadowHitGroup", {});

		//compute the size of the SBT
		UINT32 sbtSize = indirectSpecularSbtGenerator.ComputeSBTSize();

		//upload heap for the sbt
		indirectSpecularSbtResource = nv_helpers_dx12::CreateBuffer(device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		if (!indirectSpecularSbtResource)
			throw std::logic_error("Could not create SBT resource");

		//compile the sbt from the above info
		indirectSpecularSbtGenerator.Generate(indirectSpecularSbtResource.Get(), indirectSpecularRtStateObjectProps.Get());
	}


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
		GBsbtGenerator.AddRayGenerationProgram(L"GBufferRayGen", { heapPointer,(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress()
			, (void*)retargetedSequences.resource->GetGPUVirtualAddress() });
		GBsbtGenerator.AddMissProgram(L"GBufferMiss", { heapPointer });
		for (size_t i = 0; i < entities.size(); i++)
		{
			auto meshes = entities[i]->GetModel()->GetMeshes();
			for (size_t j = 0; j < meshes.size(); j++)
			{
				UINT64 materialIndex = meshes[j]->GetMaterialID();
				auto matIndexPtr = reinterpret_cast<UINT*>(materialIndex);
				GBsbtGenerator.AddHitGroup(L"GBufferHitGroup", { (void*)meshes[j]->GetVertexBufferResource()->GetGPUVirtualAddress(),
					(void*)lightingConstantBufferResource->GetGPUVirtualAddress(), (void*)lightListResource->GetGPUVirtualAddress(),heapPointer, matIndexPtr });
				GBsbtGenerator.AddHitGroup(L"ShadowHitGroup", {});
			}
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

void Game::InitNRDDenoiser()
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
		(float)renderWidth / renderHeight,	// Aspect ratio
		0.1f,				  	// Near clip plane distance
		100.0f);			  	// Far clip plane distance
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!


}

void Game::ExecuteAndResetGraphicsCommandList(ComPtr<ID3D12GraphicsCommandList> aCommandList,
	ComPtr<ID3D12CommandAllocator> aCommandAllocator[], ComPtr<ID3D12PipelineState> aPipelineState,
	ComPtr<ID3D12CommandQueue> aCommandQueue)
{
	aCommandList->Close();
	ID3D12CommandList* commandLists[] = { aCommandList.Get() };
	aCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	WaitForPreviousFrame();
	auto lol = device->GetDeviceRemovedReason();
	ThrowIfFailed(computeCommandList->Reset(aCommandAllocator[frameIndex].Get(), aPipelineState.Get()));
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

void Game::CreateTopLevelAS(const std::vector<EntityInstance>& instances, bool updateOnly)
{

	if (!updateOnly)
	{
		topLevelAsGenerator = nv_helpers_dx12::TopLevelASGenerator();
		for (int i = 0; i < instances.size(); i++)
		{
			topLevelAsGenerator.AddInstance(instances[i].bottomLevelBuffer.Get(), instances[i].modelMatrix, static_cast<UINT>(i), static_cast<UINT>(i * 2), D3D12_RAYTRACING_INSTANCE_FLAG_NONE, instances[i].instanceMask);
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
	}

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	topLevelAsGenerator.Generate(commandList.Get(),
		topLevelAsBuffers.pScratch.Get(),
		topLevelAsBuffers.pResult.Get(),
		topLevelAsBuffers.pInstanceDesc.Get(),
		updateOnly, topLevelAsBuffers.pResult.Get());

}

void Game::CreateAccelerationStructures()
{
	std::vector<AccelerationStructureBuffers> bottomLevelBuffers;

	for (int i = 0; i < entities.size(); i++)
	{
		auto meshes = entities[i]->GetModel()->GetMeshes();

		for (size_t j = 0; j < meshes.size(); j++)
		{
			AccelerationStructureBuffers blasBuffer = CreateBottomLevelAS({ meshes[j]->GetVertexBufferResourceAndCount() });
			bottomLevelBuffers.emplace_back(blasBuffer);
		}

	}

	for (UINT i = 0; i < bottomLevelBuffers.size(); i++)
	{
		RaytracingInstanceMask mask = i == 1 || i == 2 ? RAYTRACING_INSTANCE_TRANSCLUCENT : RAYTRACING_INSTANCE_OPAQUE;
		EntityInstance instance = { i ,bottomLevelBuffers[i].pResult, entities[i]->GetRawModelMatrix(), mask };
		bottomLevelBufferInstances.emplace_back(instance);
	}

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
		{1,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTIndirectDiffuseOutputTexture},
		{2,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTIndirectSpecularOutputTexture},
		{3,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTRoughnessMetalTexture},
		{4,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTPositionTexture},
		{5,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTNormalTexture},
		{6,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTAlbedoTexture},
		{7,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTTransparentOutput},
		{0,1,2,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,RaytracingHeapRangesIndices::RTMotionBuffer},
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,RaytracingHeapRangesIndices::RTAccelerationStruct},
		{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,RaytracingHeapRangesIndices::RTCameraData},
		});
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 2, MAX_LIGHTS);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_UAV, 0, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_UAV, 0, 3);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_UAV, 1, 3);

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
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 2, MAX_LIGHTS);
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
	resDes.Width = renderWidth;
	resDes.Height = renderHeight;
	resDes.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDes.MipLevels = 1;
	resDes.SampleDesc.Count = 1;
	resDes.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtOutPut.resource.GetAddressOf())));
	rtOutPut.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtOutPut.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtTransparentOutput.resource.GetAddressOf())));
	rtTransparentOutput.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtTransparentOutput.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtIndirectDiffuseOutPut.resource.GetAddressOf())));
	rtIndirectDiffuseOutPut.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtIndirectDiffuseOutPut.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtIndirectSpecularOutPut.resource.GetAddressOf())));
	rtIndirectSpecularOutPut.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtIndirectSpecularOutPut.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(tempRTIndDiffuse.resource.GetAddressOf())));
	tempRTIndDiffuse.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	tempRTIndDiffuse.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(tempRTIndSpec.resource.GetAddressOf())));
	tempRTIndSpec.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	tempRTIndSpec.resourceType = RESOURCE_TYPE_UAV;

	resDes.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtNormals.resource.GetAddressOf())));
	rtNormals.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtNormals.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtRoughnessMetal.resource.GetAddressOf())));
	rtRoughnessMetal.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtRoughnessMetal.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtPosition.resource.GetAddressOf())));
	rtPosition.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtPosition.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(rtAlbedo.resource.GetAddressOf())));
	rtAlbedo.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	rtAlbedo.resourceType = RESOURCE_TYPE_UAV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(prevPosition.resource.GetAddressOf())));
	prevPosition.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	prevPosition.resourceType = RESOURCE_TYPE_SRV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(prevNormals.resource.GetAddressOf())));
	prevNormals.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	prevNormals.resourceType = RESOURCE_TYPE_SRV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(prevNoisy.resource.GetAddressOf())));
	prevNoisy.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	prevNoisy.resourceType = RESOURCE_TYPE_SRV;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(currentNoisy.resource.GetAddressOf())));
	currentNoisy.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	currentNoisy.resourceType = RESOURCE_TYPE_SRV;

	resDes.Format = DXGI_FORMAT_R8_UINT;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(acceptBools.resource.GetAddressOf())));
	acceptBools.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	acceptBools.resourceType = RESOURCE_TYPE_UAV;

	resDes.Format = DXGI_FORMAT_R32G32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(outPrevFramePixels.resource.GetAddressOf())));
	outPrevFramePixels.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	outPrevFramePixels.resourceType = RESOURCE_TYPE_UAV;
}

void Game::CreateRaytracingDescriptorHeap()
{
	//creating the descriptor heap, it will contain two descriptors
	//one for the UAV output and an SRV for the acceleration structure
	rtDescriptorHeap.Create(10000, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	rtDescriptorHeap.CreateDescriptor(rtOutPut, rtOutPut.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtIndirectDiffuseOutPut, rtOutPut.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtIndirectSpecularOutPut, rtOutPut.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtRoughnessMetal, rtRoughnessMetal.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtPosition, rtPosition.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtNormals, rtNormals.resourceType);
	rtDescriptorHeap.CreateDescriptor(rtAlbedo, rtAlbedo.resourceType);
	rtDescriptorHeap.CreateDescriptor(velocityBuffer, RESOURCE_TYPE_UAV);
	rtDescriptorHeap.CreateDescriptor(rtTransparentOutput, rtTransparentOutput.resourceType);
	renderTargetSRVHeap.CreateDescriptor(rtOutPut, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(rtIndirectDiffuseOutPut, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(rtIndirectSpecularOutPut, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(rtTransparentOutput, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(prevPosition, prevPosition.resourceType, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(prevNormals, prevNormals.resourceType, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(prevNoisy, prevNoisy.resourceType, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(currentNoisy, currentNoisy.resourceType, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(acceptBools, acceptBools.resourceType, 0, renderWidth, renderHeight, 0, 0);
	renderTargetSRVHeap.CreateDescriptor(outPrevFramePixels, outPrevFramePixels.resourceType, 0, renderWidth, renderHeight, 0, 0);
	renderTargetSRVHeap.CreateDescriptor(rtPosition, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(rtNormals, RESOURCE_TYPE_SRV, 0, renderWidth, renderHeight, 0, 1);
	renderTargetSRVHeap.CreateDescriptor(tempRTIndDiffuse, RESOURCE_TYPE_UAV);
	renderTargetSRVHeap.CreateDescriptor(tempRTIndSpec, RESOURCE_TYPE_UAV);
	renderTargetSRVHeap.CreateDescriptor(depthTex, RESOURCE_TYPE_SRV, 0, 0, 0, 0, 1);



	//initializing the camera buffer used for raytracing
	//ThrowIfFailed(device->CreateCommittedResource(&nv_helpers_dx12::kUploadHeapProps, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		//D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(cameraData.resource.GetAddressOf())));

	rtDescriptorHeap.CreateRaytracingAccelerationStructureDescriptor(topLevelAsBuffers);
	rtDescriptorHeap.CreateDescriptor(cameraData, RESOURCE_TYPE_CBV, AlignUp(sizeof(RayTraceCameraData), 256u));
	rtDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/skybox5.hdr", skyboxTexResource, RESOURCE_TYPE_SRV, TEXTURE_TYPE_HDR, false);

	static UINT numStaticResources = 0;
	for (int i = 0; i < materials.size(); i++)
	{
		auto cpuHandle = rtDescriptorHeap.GetCPUHandle(rtDescriptorHeap.GetLastResourceIndex());
		auto otherCPUHandle = materials[i]->GetDescriptorHeap().GetCPUHandle(0);
		numStaticResources += 4;
		rtDescriptorHeap.IncrementLastResourceIndex(4);
		device->CopyDescriptorsSimple(4, cpuHandle, otherCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	rtCamera.prevProj = mainCamera->GetProjectionMatrix();
	rtCamera.prevView = mainCamera->GetViewMatrix();
	ZeroMemory(&rtCamera, sizeof(rtCamera));
	ThrowIfFailed(cameraData.resource->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&cameraBufferBegin)));
	memcpy(cameraBufferBegin, &rtCamera, sizeof(rtCamera));

}

void Game::CreateRayTracingPipeline()
{
	CreateRayTracingDirectLightingPipeline();
	CreateRaytracingTransparencyPipeline();
	CreateRayTracingIndirectDiffusePipeline();
	CreateRayTracingIndirectSpecularPipeline();
}

void Game::CreateRayTracingDirectLightingPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	rayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../RayGen.hlsl");
	missLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Miss.hlsl");
	hitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Hit.hlsl");


	//add the libraies to pipeliene
	pipeline.AddLibrary(rayGenLib.Get(), { L"RayGen" });
	pipeline.AddLibrary(missLib.Get(), { L"Miss" });
	pipeline.AddLibrary(hitLib.Get(), { L"ClosestHit",L"PlaneClosestHit" });

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
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"HitGroup" ,L"PlaneHitGroup",L"ShadowHitGroup" }); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
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

void Game::CreateRaytracingTransparencyPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	transparencyRayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../RayGenTransparency.hlsl");
	missLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Miss.hlsl");
	hitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Hit.hlsl");


	//add the libraies to pipeliene
	pipeline.AddLibrary(transparencyRayGenLib.Get(), { L"RayGenTransparency" });
	pipeline.AddLibrary(missLib.Get(), { L"Miss" });
	pipeline.AddLibrary(hitLib.Get(), { L"ClosestHit",L"PlaneClosestHit" });

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
	pipeline.AddRootSignatureAssociation(rayGenRootSig.Get(), { L"RayGenTransparency" });
	pipeline.AddRootSignatureAssociation(missRootSig.Get(), { L"Miss",L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"HitGroup" ,L"PlaneHitGroup",L"ShadowHitGroup" }); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
	//pipeline.AddRootSignatureAssociation(shadowRootSig.Get(), { L"ShadowHitGroup" });

	//payload size defines the maximum size of the data carried by the rays
	pipeline.SetMaxPayloadSize(20 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(30);

	//creating the state obj
	rtTransparentStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(rtTransparentStateObject->QueryInterface(IID_PPV_ARGS(rtTransparentStateObjectProps.GetAddressOf())));
}

void Game::CreateRayTracingIndirectDiffusePipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	indirectDiffuseRayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../RayGenIndirectDiffuse.hlsl");
	missLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Miss.hlsl");
	hitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Hit.hlsl");


	//add the libraies to pipeliene
	pipeline.AddLibrary(indirectDiffuseRayGenLib.Get(), { L"IndirectDiffuseRayGen" });
	pipeline.AddLibrary(missLib.Get(), { L"Miss" });
	pipeline.AddLibrary(hitLib.Get(), { L"ClosestHit",L"PlaneClosestHit" });

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
	pipeline.AddRootSignatureAssociation(rayGenRootSig.Get(), { L"IndirectDiffuseRayGen" });
	pipeline.AddRootSignatureAssociation(missRootSig.Get(), { L"Miss",L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"HitGroup" ,L"PlaneHitGroup",L"ShadowHitGroup" }); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
	//pipeline.AddRootSignatureAssociation(shadowRootSig.Get(), { L"ShadowHitGroup" });

	//payload size defines the maximum size of the data carried by the rays
	pipeline.SetMaxPayloadSize(20 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(30);

	//creating the state obj
	indirectDiffuseRtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(indirectDiffuseRtStateObject->QueryInterface(IID_PPV_ARGS(indirectDiffuseRtStateObjectProps.GetAddressOf())));
}

void Game::CreateRayTracingIndirectSpecularPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(device.Get());

	//the raytracing pipeline contains all the shader code
	indirectSpecularRayGenLib = nv_helpers_dx12::CompileShaderLibrary(L"../../RayGenIndirectSpecular.hlsl");
	missLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Miss.hlsl");
	hitLib = nv_helpers_dx12::CompileShaderLibrary(L"../../Hit.hlsl");


	//add the libraies to pipeliene
	pipeline.AddLibrary(indirectSpecularRayGenLib.Get(), { L"IndirectSpecularRayGen" });
	pipeline.AddLibrary(missLib.Get(), { L"Miss" });
	pipeline.AddLibrary(hitLib.Get(), { L"ClosestHit",L"PlaneClosestHit" });

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
	pipeline.AddRootSignatureAssociation(rayGenRootSig.Get(), { L"IndirectSpecularRayGen" });
	pipeline.AddRootSignatureAssociation(missRootSig.Get(), { L"Miss",L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(closestHitRootSignature.Get(), { L"HitGroup" ,L"PlaneHitGroup",L"ShadowHitGroup" }); // the intersection, anyhit, and closest hit shaders are bundled together in a hit group
	//pipeline.AddRootSignatureAssociation(shadowRootSig.Get(), { L"ShadowHitGroup" });

	//payload size defines the maximum size of the data carried by the rays
	pipeline.SetMaxPayloadSize(20 * sizeof(float));

	//max attrrib size, for now I am using the built in triangle attribs
	pipeline.SetMaxAttributeSize(4 * sizeof(float));

	//setting the recursion depth
	pipeline.SetMaxRecursionDepth(30);

	//creating the state obj
	indirectSpecularRtStateObject = pipeline.Generate();

	// Cast the state object into a properties object, allowing to later access
	// the shader pointers by name
	ThrowIfFailed(indirectSpecularRtStateObject->QueryInterface(IID_PPV_ARGS(indirectSpecularRtStateObjectProps.GetAddressOf())));
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

	dynamicBufferRing.OnBeginFrame();

	auto kb = keyboard->GetState();
	auto mouseState = mouse->GetState();
	float xRand = (jitters[numFrames % 16].x) / (float(renderWidth));
	float yRand = (jitters[numFrames % 16].y) / (float(renderHeight));

	//xRand = xRand / renderWidth;
	//yRand = yRand / renderHeight;

	//mainCamera->JitterProjMatrix(xRand, yRand);

	currentJitters = Vector2(xRand, yRand);

	// Quit if the escape key is pressed
	if (kb.Escape)
		Quit();

	keys.Update(kb);
	mouseButtons.Update(mouseState);

	if (addNewEntity)
	{
		auto newEnt = std::make_shared<Entity>(registry, pbrPipelineState, rootSignature, ("entity "+std::to_string(entities.size())));
		newEnt->PrepareConstantBuffers(residencyManager, residencySet);
		entities.emplace_back(newEnt);
		entityNames[entities.size()-1] = entities[entities.size() - 1]->GetTag();
		addNewEntity = false;
	}
	
	numFrames++;
	mainCamera->Update(deltaTime, mouse, keyboard);

	bmfrPreProcData.view = mainCamera->GetViewMatrix();
	bmfrPreProcData.proj = mainCamera->GetProjectionMatrix();
	bmfrPreProcData.frame_number = numFrames;

	memcpy(bmfrPreprocessBegin, &bmfrPreProcData, sizeof(bmfrPreProcData));

	if (mouseButtons.leftButton == DirectX::Mouse::ButtonStateTracker::RELEASED && !entityManipulated)
	{
		pickingIndex = -1;
		bool intersects = false;
		Vector4 origin;
		Vector4 dir;
		mainCamera->GetRayOriginAndDirection(mouseState.x, mouseState.y, width, height ,origin, dir);
		float minDist = FLT_MAX;
		for (size_t i = 0; i < entities.size(); i++)
		{
			float objSpaceDist = 0;
			intersects = entities[i]->RayBoundIntersection(origin, dir, objSpaceDist, mainCamera->GetViewMatrix());

			if (intersects)
			{
				auto distance = Vector3::Distance(entities[i]->GetPosition(), mainCamera->GetPosition());

				if (minDist >= distance)
				{
					minDist = distance;
					pickingIndex = i;
				}
			}

		}

	}


	if (!mouse->GetState().rightButton)
	{
		if (keys.released.W)
		{
			gizmoMode = ImGuizmo::TRANSLATE;
		}

		else if (keys.released.E)
		{
			gizmoMode = ImGuizmo::ROTATE;
		}

		else if (keys.released.R)
		{
			gizmoMode = ImGuizmo::SCALE;
		}
	}

	lightData.cameraPosition = mainCamera->GetPosition();
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	lightingData.cameraPosition = mainCamera->GetPosition();
	lightingData.lightCount = lightCount;
	lightingData.cameraForward = mainCamera->GetDirection();
	lightingData.totalTime += deltaTime;
	lightingData.fogDense = fogDensity;

	lightCullingExternData.view = mainCamera->GetViewMatrix();
	lightCullingExternData.projection = mainCamera->GetProjectionMatrix();
	lightCullingExternData.inverseProjection = mainCamera->GetInverseProjection();
	lightCullingExternData.lightCount = lightCount;
	lightCullingExternData.cameraPosition = mainCamera->GetPosition();

	memcpy(lightingCbufferBegin, &lightingData, sizeof(lightingData));
	memcpy(lightBufferBegin, lights, sizeof(Light) * MAX_LIGHTS);
	memcpy(lightCullingExternBegin, &lightCullingExternData, sizeof(lightCullingExternData));

	for (size_t i = 0; i < entities.size(); i++)
	{
		entities[i]->Update(deltaTime);

		if(isRaytracingAllowed)
			bottomLevelBufferInstances[i].modelMatrix = entities[i]->GetRawModelMatrix();
	}

	if (isRaytracingAllowed)
	{

		rtCamera.view = mainCamera->GetViewMatrix();
		rtCamera.proj = mainCamera->GetProjectionMatrix();
		rtCamera.doRestir = doRestir;
		rtCamera.doRestirGI = doRestirGI;
		rtCamera.doOutput = !restirSpatialReuse;
		XMMATRIX viewTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.view));
		XMStoreFloat4x4(&rtCamera.iView, XMMatrixTranspose(XMMatrixInverse(nullptr, viewTranspose)));
		XMMATRIX projTranspose = XMMatrixTranspose(XMLoadFloat4x4(&rtCamera.proj));
		XMStoreFloat4x4(&rtCamera.iProj, XMMatrixTranspose(XMMatrixInverse(nullptr, projTranspose)));
		//
		memcpy(cameraBufferBegin, &rtCamera, sizeof(rtCamera));
		CreateTopLevelAS(bottomLevelBufferInstances, true);
	}

	for (int i = 0; i < emitters.size(); i++)
	{
		//emitters[i]->UpdateParticles(deltaTime, totalTime);
	}



	//if (!raster)
	//{
		//CreateAccelerationStructures();
		//rtDescriptorHeap.UpdateRaytracingAccelerationStruct(device, topLevelAsBuffers);
	//}

	//FlockingSystem::FlockerSystem(registry, flockers, deltaTime);


	velocityBufferData.projection = mainCamera->GetProjectionMatrix();
	velocityBufferData.view = mainCamera->GetViewMatrix();

	for (size_t i = 0; i < entities.size(); i++)
	{
		entityNames[i] = entities[i]->GetTag();
	}

	//for (size_t i = 0; i < flockers.size(); i++)
	//{
	//	flockers[i]->Update(deltaTime);
	//}

	UpdateGUI(deltaTime, totalTime);
}

void Game::UpdateGUI(float deltaTime, float totalTime)
{

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	PopulateCommandList();

	if (true)
	{
		ID3D12CommandList* pcommandLists[] = { computeCommandList.Get() };
		computeCommandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);
		auto lol = device->GetDeviceRemovedReason();

		ThrowIfFailed(computeCommandQueue->Signal(computeFence.Get(), fenceValues[frameIndex]));

		ThrowIfFailed(commandQueue->Wait(computeFence.Get(), fenceValues[frameIndex]));
	}

	//execute the commanf list
	ID3D12CommandList* pcommandLists[] = { commandList.Get() };
	D3DX12Residency::ResidencySet* ppSets[] = { residencySet.get() };
	commandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);
	//residencyManager.ExecuteCommandLists(commandQueue.Get(), pcommandLists, ppSets, 1);
	//present the frame
	ThrowIfFailed(swapChain->Present(1, 0));

	MoveToNextFrame();


	velocityBufferData.prevView = mainCamera->GetViewMatrix();
	velocityBufferData.prevProjection = mainCamera->GetProjectionMatrix();

	taaData.prevView = mainCamera->GetViewMatrix();
	taaData.prevProjection = mainCamera->GetProjectionMatrix();

	bmfrPreProcData.prevProj = mainCamera->GetProjectionMatrix();
	bmfrPreProcData.view = mainCamera->GetViewMatrix();

	rtCamera.prevProj = mainCamera->GetProjectionMatrix();
	rtCamera.prevView = mainCamera->GetViewMatrix();
	rtCamera.frameCount = frameCount;
	const FLOAT clearValue[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
	//commandList->ClearUnorderedAccessViewFloat(rtOutPut.uavGPUHandle, rtOutPut.uavCPUHandle, rtOutPut.resource.Get(), clearValue, 0, nullptr);

	prevJitters = currentJitters;

}

void Game::RenderGUI(float deltaTime, float totalTime)
{

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame(ImVec2(1920, 1080));



	ImGuiIO& io = ImGui::GetIO();

	auto rtvHandle = rtvDescriptorHeap.GetCPUHandle(frameIndex);
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	//setting the constant buffer descriptor table
	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeapPtr() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//raytrace
	{
		ImGui::Begin("Render Mode");
		ImGui::Checkbox("Raster", &raster);
		if (!raster)
		{
			ImGui::Checkbox("Use ReStir", &doRestir);

			if (doRestir || doRestirGI)
			{
				ImGui::Checkbox("Use Spatial Reuse", &restirSpatialReuse);
			}

			
			ImGui::Checkbox("Use Restir GI", &doRestirGI);

			ImGui::SliderFloat("FogDesnity", &fogDensity, 0, 1.0f);
			
		}
		ImGui::End();
	}


	if(pickingIndex!=-1)
		entityManipulated = entities[pickingIndex]->ManipulateTransforms(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), gizmoMode);

	ImGui::Begin("Scene Objects");
	{
		if (ImGui::BeginPopupContextItem("item context menu"))
		{
			addNewEntity = ImGui::Selectable("Add New Entity");
			ImGui::EndPopup();
		}
		for (size_t i = 0; i < entities.size(); i++)
		{
			
			bool selected = false;
			if (i == pickingIndex)
				selected = true;
			auto temp = entityNames[i];
			auto id = entt::to_integral(entities[i]->GetEntityID());

			if (ImGui::SelectableInput((char*)std::to_string(id).c_str(), selected, 0, temp))
			{

				if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
				{
					int n_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
					if (n_next >= 0 && n_next < entities.size())
					{
						auto newName = entityNames[i];
						entityNames[i] = entityNames[n_next];
						entityNames[n_next] = temp;
						ImGui::ResetMouseDragDelta();
					}
				}
				pickingIndex = i;
				entities[i]->SetTag(temp);
			}

		}
	}
	ImGui::End();

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove;
	ImGui::SetNextWindowBgAlpha(0);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::Begin("GameWindow", NULL, flags);
	{
		if (ImGui::IsWindowDocked())
		{
			ImGui::SetNextWindowBgAlpha(0);
		}
		if (ImGui::IsWindowHovered())
		{
			ImGui::CaptureMouseFromApp(false);
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
	flags = 0;
	//ImGui::SetNextWindowBgAlpha(1.0f);
	//io.WantCaptureMouse = true;
	//flags = ImGuiWindowFlags_NoBackground;

	ImGui::Begin("Component Viewer");
	{
		if (pickingIndex != -1)
		{
			static bool visible = true;
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{

				if (ImGui::RadioButton("Translate", gizmoMode == ImGuizmo::TRANSLATE))
					gizmoMode = ImGuizmo::TRANSLATE;
				ImGui::SameLine();
				if (ImGui::RadioButton("Rotate", gizmoMode == ImGuizmo::ROTATE))
					gizmoMode = ImGuizmo::ROTATE;
				ImGui::SameLine();
				if (ImGui::RadioButton("Scale", gizmoMode == ImGuizmo::SCALE))
					gizmoMode = ImGuizmo::SCALE;

				Vector4 trs = Vector4(entities[pickingIndex]->GetPosition());
				Vector3 scal = entities[pickingIndex]->GetScale();
				Quaternion rot = entities[pickingIndex]->GetRotation();

				auto objMat = entities[pickingIndex]->GetModelMatrix();

				auto angles = ToEulerAngles(rot);

				float matrixTranslation[] = { trs.x, trs.y, trs.z };
				float matrixRotation[] = { angles.pitch, angles.yaw, angles.roll };
				float matrixScale[] = { scal.x, scal.y, scal.z };

				gizmoMode = ImGui::DragFloat3("Position", matrixTranslation) ? gizmoMode = ImGuizmo::TRANSLATE : gizmoMode;
				gizmoMode = ImGui::DragFloat3("Rotation", matrixRotation) ? gizmoMode = ImGuizmo::ROTATE : gizmoMode;
				gizmoMode = ImGui::DragFloat3("Scale", matrixScale) ? gizmoMode = ImGuizmo::SCALE : gizmoMode;


				entities[pickingIndex]->SetPosition(Vector3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]));
				entities[pickingIndex]->SetScale(Vector3(matrixScale[0], matrixScale[1], matrixScale[2]));
				entities[pickingIndex]->SetRotation(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
			}




			//ImGui::SetNextWindowBgAlpha(1.0f);

			if (ImGui::CollapsingHeader("Material Viewer"))
			{
				if (pickingIndex != -1)
				{
					//create our ImGui window
					bool close = true;
					//get the mouse position
					ImVec2 pos = ImGui::GetCursorScreenPos();

					auto io = ImGui::GetIO();
					auto meshes = entities[pickingIndex]->GetModel()->GetMeshes();

					auto id = meshes[0]->GetMaterialID();
					int h = 128;
					int w = 128;

					auto handle = gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(id);
					ImGui::Text("Albedo");
					ImGui::Image((ImTextureID)handle.ptr, ImVec2((float)w, (float)h));
					handle = gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(id + 1);
					ImGui::Text("Normal");
					ImGui::Image((ImTextureID)handle.ptr, ImVec2((float)w, (float)h));

					handle = gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(id + 2);
					ImGui::Text("Roughness");
					ImGui::Image((ImTextureID)handle.ptr, ImVec2((float)w, (float)h));
					handle = gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(id + 3);
					ImGui::Text("Metalness");
					ImGui::Image((ImTextureID)handle.ptr, ImVec2((float)w, (float)h));

				}
			}

			if (ImGui::Button("Add Model"))
			{
				entities[pickingIndex]->AddModel("../../Assets/Models/sphere.obj");
			}
		}
	}
	ImGui::End();


	
	// Rendering
	ImGui::Render();

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
}

void Game::RenderEditorWindow()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame(ImVec2(1920, 1080));
	
	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = editorWindowTarget.rtvCPUHandle;//(rtvDescriptorHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart(),
	
	// Render Dear ImGui graphics
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);

	if (pickingIndex != -1)
	{
		entities[pickingIndex]->ManipulateTransforms(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), gizmoMode);
	}


	// Rendering
	ImGui::Render();
	//setting the constant buffer descriptor table
	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeapPtr() };
	
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
}

void Game::CreateFSRPass()
{


}

void Game::RaytracingPrePass()
{
	//doing the necessary copies
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(rtPosition.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	//doing the necessary copies
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(rtNormals.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	//doing the necessary copies
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(prevPosition.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &transition);

	 transition = CD3DX12_RESOURCE_BARRIER::Transition(prevNormals.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &transition);

	commandList->CopyResource(prevPosition.resource.Get(), rtPosition.resource.Get());

	commandList->CopyResource(prevNormals.resource.Get(), rtNormals.resource.Get());

	//doing the necessary copies
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(rtPosition.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &transition);

	//doing the necessary copies
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(rtNormals.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &transition);

	//doing the necessary copies
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(prevPosition.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);

	 transition = CD3DX12_RESOURCE_BARRIER::Transition(prevNormals.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
}

void Game::DepthPrePass()
{

	//skybox->CreateEnvironment(commandList, device, skyboxRootSignature, skyboxRootSignature, irradiencePSO, prefilteredMapPSO, brdfLUTPSO, dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset));

	TransitionManagedResource(commandList, depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//set necessary state
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//setting the constant buffer descriptor table
	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap.GetCPUHandle(frameIndex);//(rtvDescriptorHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart(),
		//frameIndex,rtvDescriptorSize);
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &depthTex.dsvCPUHandle);
	commandList->ClearDepthStencilView(depthTex.dsvCPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	/**/

	//record commands
	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetBeginningStaticResourceOffset();//(mainBufferHeap->GetGPUDescriptorHandleForHeapStart(),0,cbvDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityMaterials, gpuCBVSRVUAVHandle);
	gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetStaticDescriptorOffset();
	for (UINT i = 0; i < entities.size(); i++)
	{
		entities[i]->PrepareMaterial(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());
		commandList->SetPipelineState(depthPrePassPipelineState.Get());
		entities[i]->Draw(commandList, gpuHeapRingBuffer);
	}

	//for (UINT i = 0; i < flockers.size(); i++)
	//{
	//	flockers[i]->PrepareMaterial(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());
	//	commandList->SetPipelineState(depthPrePassPipelineState.Get());
	//	flockers[i]->Draw(device, commandList, gpuHeapRingBuffer);
	//}

	TransitionManagedResource(commandList, depthTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(visibleLightIndicesBuffer.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &transition);
	visibleLightIndicesBuffer.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	auto lol = commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForPreviousFrame();

	ThrowIfFailed(
		commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));
}

void Game::BNDSPrePass()
{
	computeCommandList->SetComputeRootSignature(bndsComputeRootSignature.Get());
	computeCommandList->SetPipelineState(bndsPipelineState.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };
	computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	TransitionManagedResource(commandList, rtCombineOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	TransitionManagedResource(commandList, blueNoiseTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	TransitionManagedResource(commandList, retargetTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	TransitionManagedResource(commandList, sharpenOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


	auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(sampleSequences.resource.Get());
	computeCommandList->ResourceBarrier(1, &barrier);
	computeCommandList->SetComputeRootDescriptorTable(BlueNoiseDithering::BlueNoiseTex, blueNoiseTex.srvGPUHandle);
	computeCommandList->SetComputeRootDescriptorTable(BlueNoiseDithering::PrevFrameNoisy, rtCombineOutput.srvGPUHandle);
	computeCommandList->SetComputeRootDescriptorTable(BlueNoiseDithering::RetargetTex, retargetTex.srvGPUHandle);
	computeCommandList->SetComputeRootUnorderedAccessView(BlueNoiseDithering::NewSequences, sampleSequences.resource->GetGPUVirtualAddress());
	computeCommandList->SetComputeRootUnorderedAccessView(BlueNoiseDithering::RetargettedSequencesBNDS, retargetedSequences.resource->GetGPUVirtualAddress());

	UINT frameCount = numFrames;

	if (raster)
	{
		frameCount = 0;
	}

	bndsData = {};
	bndsData.frame = frameCount;

	memcpy(bndsDataBegin, &bndsData, sizeof(BNDSExternalData));

	ID3D12ShaderReflection* refl;

	
	computeCommandList->SetComputeRootConstantBufferView(BlueNoiseDithering::FrameNum, bndsCBResource->GetGPUVirtualAddress());
	computeCommandList->Dispatch(renderWidth / 4, renderHeight / 4, 1);

	barrier = CD3DX12_RESOURCE_BARRIER::UAV(sampleSequences.resource.Get());
	commandList->ResourceBarrier(1, &barrier);
}

void Game::BNDSRetargetingPass()
{
	computeCommandList->SetComputeRootSignature(retargetingRootSignature.Get());
	computeCommandList->SetPipelineState(retargetingPipelineState.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };
	computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	TransitionManagedResource(commandList, retargetTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeCommandList->SetComputeRootDescriptorTable(RetargetingPass::RetargetTexture, retargetTex.srvGPUHandle);
	computeCommandList->SetComputeRootUnorderedAccessView(RetargetingPass::OldSequences, sampleSequences.resource->GetGPUVirtualAddress());
	computeCommandList->SetComputeRootUnorderedAccessView(RetargetingPass::RetargetedSequences, retargetedSequences.resource->GetGPUVirtualAddress());

	UINT frameCount = numFrames;

	if (raster)
	{
		frameCount = 0;
	}

	bndsData = {};
	bndsData.frame = frameCount;

	memcpy(bndsDataBegin, &bndsData, sizeof(BNDSExternalData));

	computeCommandList->SetComputeRootConstantBufferView(RetargetingPass::RetargetingPassCBV, bndsCBResource->GetGPUVirtualAddress());
	computeCommandList->Dispatch(renderWidth / 4, renderHeight / 4, 1);

}

void Game::RenderVelocityBuffer()
{
	TransitionManagedResource(commandList, velocityBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	auto rtvHandle = velocityBuffer.rtvCPUHandle;

	const float clearColor[] = { 0.0f, 0.0f, 0.f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto dscCPUHandle = depthStencilBuffer2.dsvCPUHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dscCPUHandle);
	commandList->ClearDepthStencilView(dscCPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->SetGraphicsRootSignature(velRootSig.Get());
	commandList->SetPipelineState(velPSO.Get());

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	commandList->SetGraphicsRoot32BitConstants(1, 2, &currentJitters, 0);
	commandList->SetGraphicsRoot32BitConstants(1, 2, &prevJitters, 2);

	for (UINT i = 0; i < entities.size(); i++)
	{

		velocityBufferData.world = entities[i]->GetModelMatrix();
		velocityBufferData.prevWorld = entities[i]->GetPrevModelMatrix();

		{
			D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;

			uint32_t* pDataBegin = 0;

			dynamicBufferRing.AllocConstantBuffer(sizeof(VelocityConstantBuffer), reinterpret_cast<void**>(&pDataBegin), &gpuAddr);

			memcpy(pDataBegin, &velocityBufferData, sizeof(VelocityConstantBuffer));
			commandList->SetGraphicsRootConstantBufferView(0, gpuAddr);
		}

		auto model = entities[i]->GetModel();

		if (model != nullptr)
		{
			model->Draw(commandList, false);
		}

	}

	//transition render target to readable texture and then transition it back to render target
	TransitionManagedResource(commandList, velocityBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


}

void Game::LightCullingPass()
{
	computeCommandList->SetComputeRootSignature(computeRootSignature.Get());
	computeCommandList->SetPipelineState(computePipelineState.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };
	computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	TransitionManagedResource(commandList, depthTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	computeCommandList->SetComputeRootDescriptorTable(LightCullingRootIndices::DepthMapSRV, depthTex.srvGPUHandle);
	computeCommandList->SetComputeRootShaderResourceView(LightCullingRootIndices::LightListSRV, lightListResource->GetGPUVirtualAddress());
	computeCommandList->SetComputeRootUnorderedAccessView(LightCullingRootIndices::VisibleLightIndicesUAV, visibleLightIndicesBuffer.resource->GetGPUVirtualAddress());
	computeCommandList->SetComputeRootConstantBufferView(LightCullingRootIndices::LightCullingExternalDataCBV, lightCullingCBVResource->GetGPUVirtualAddress());

	computeCommandList->Dispatch(renderWidth / TILE_SIZE, renderHeight / TILE_SIZE, 1);

	//ThrowIfFailed(computeCommandList->Close());

	//SubmitComputeCommandList(computeCommandList, commandList);

}


void Game::PopulateCommandList()
{

	residencySet->Open();


	auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(retargetedSequences.resource.Get());

	commandList->ResourceBarrier(1, &uavBarrier);


	BNDSPrePass();

	uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sampleSequences.resource.Get());

	computeCommandList->ResourceBarrier(1, &uavBarrier);

	BNDSRetargetingPass();

	uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(retargetedSequences.resource.Get());

	commandList->ResourceBarrier(1, &uavBarrier);

	RenderVelocityBuffer();
	
	DepthPrePass();
	
	LightCullingPass();

	//set necessary state
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//setting the constant buffer descriptor table
	ID3D12DescriptorHeap* ppHeaps[] = { gpuHeapRingBuffer->GetDescriptorHeap().GetHeap().Get() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].resource.Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	//indicate that the back buffer is the render target
	commandList->ResourceBarrier(1,
		&transition);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(taaInput.rtvCPUHandle);//(rtvDescriptorHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart(),
		//frameIndex,rtvDescriptorSize);
	auto dscCPUHandle = dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dscCPUHandle);
	commandList->ClearDepthStencilView(dsDescriptorHeap.GetCPUHandle(depthStencilBuffer.heapOffset), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	auto projectionMat = mainCamera->GetProjectionMatrix();

	if (raster)
	{
		TransitionManagedResource(commandList, taaInput, D3D12_RESOURCE_STATE_RENDER_TARGET);

		//record commands
		const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetBeginningStaticResourceOffset();//(mainBufferHeap->GetGPUDescriptorHandleForHeapStart(),0,cbvDescriptorSize);
		commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityMaterials, gpuCBVSRVUAVHandle);
		gpuCBVSRVUAVHandle = gpuHeapRingBuffer->GetStaticDescriptorOffset();

		commandList->SetGraphicsRootConstantBufferView(EntityRootIndices::EntityPixelCBV, lightingConstantBufferResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(EntityRootIndices::EntityLightListSRV, lightListResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(EntityRootIndices::EntityLightIndices, visibleLightIndicesBuffer.resource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityEnvironmentSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(skybox->environmentTexturesIndex));
		commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityLTCSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(ltcLUT.heapOffset));
		commandList->SetGraphicsRoot32BitConstant(EntityRootIndices::EnableIndirectLighting, raster, 0);
		commandList->SetGraphicsRoot32BitConstant(EntityRootIndices::EnableIndirectLighting, inlineRaytracing, 1);

		if(isRaytracingAllowed)
			commandList->SetGraphicsRootShaderResourceView(EntityRootIndices::AccelerationStructureSRV, topLevelAsBuffers.pResult->GetGPUVirtualAddress());

		for (UINT i = 0; i < entities.size(); i++)
		{
			entities[i]->PrepareMaterial(mainCamera->GetViewMatrix(), projectionMat);
			commandList->SetPipelineState(entities[i]->GetPipelineState().Get());
			entities[i]->Draw(commandList, gpuHeapRingBuffer);
		}

		commandList->SetGraphicsRootSignature(entity6->GetRootSignature().Get());
		entity6->PrepareMaterial(mainCamera->GetViewMatrix(), projectionMat);
		commandList->SetPipelineState(entity6->GetPipelineState().Get());

		auto cameraPos = mainCamera->GetPosition();
		commandList->SetGraphicsRootDescriptorTable(InteriorMappingRootIndices::TextureArraySRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(interiorMaterial->materialIndex));
		commandList->SetGraphicsRoot32BitConstants(InteriorMappingRootIndices::ExternDataPSCBV, 3, &cameraPos, 0);
		commandList->SetGraphicsRoot32BitConstant(InteriorMappingRootIndices::ExternDataPSCBV, 5, 3);
		commandList->SetGraphicsRoot32BitConstant(InteriorMappingRootIndices::ExternDataPSCBV, 5, 4);
		commandList->SetGraphicsRoot32BitConstant(InteriorMappingRootIndices::ExternDataPSCBV, 11, 5);
		commandList->SetGraphicsRootDescriptorTable(InteriorMappingRootIndices::ExteriorTextureSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(interiorMaterial->materialIndex+1));
		commandList->SetGraphicsRootDescriptorTable(InteriorMappingRootIndices::CapTextureSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(interiorMaterial->materialIndex + 2));
		commandList->SetGraphicsRootDescriptorTable(InteriorMappingRootIndices::SDFTextureSRV, gpuHeapRingBuffer->GetDescriptorHeap().GetGPUHandle(interiorMaterial->materialIndex + 3));

		//entity6->Draw(device, commandList, gpuHeapRingBuffer);


		//for (UINT i = 0; i < flockers.size(); i++)
		//{
		//	flockers[i]->PrepareMaterial(mainCamera->GetViewMatrix(), projectionMat);
		//	commandList->SetPipelineState(flockers[i]->GetPipelineState().Get());
		//	flockers[i]->Draw(device, commandList, gpuHeapRingBuffer);
		//}

		skybox->PrepareForDraw(mainCamera->GetViewMatrix(), projectionMat, mainCamera->GetPosition());

		commandList->ExecuteBundle(skyboxBundle.Get());

		flame->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition(), totalTime);
		flame->Render(gpuHeapRingBuffer);

		for (int i = 0; i < emitters.size(); i++)
		{
			//emitter1->Draw(commandList, gpuHeapRingBuffer, mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), totalTime);
		}

		if(raster)
			RenderPostProcessing(taaInput);

	}

	if (!raster && isRaytracingAllowed)
	{
		RaytracingPrePass();

		ID3D12DescriptorHeap* ppHeaps[] = { rtDescriptorHeap.GetHeap().Get() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		TransitionManagedResource(commandList, rtCombineOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);
		TransitionManagedResource(commandList, velocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		rtvHandle = rtCombineOutput.rtvCPUHandle;
		const float clearColor[] = { 0.6f, 0.8f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		CreateGBufferRays();
		CreateDirectRays();
		//CreateIndirectDiffuseRays();
		//CreateIndirectSpecularRays();

		CreateTransparencyRays();

		uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(retargetedSequences.resource.Get());

		commandList->ResourceBarrier(1, &uavBarrier);


		uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sampleSequences.resource.Get());

		commandList->ResourceBarrier(1, &uavBarrier);

		//RT combine
		{
			//transition render target to readable texture and then transition it back to render target
			TransitionManagedResource(commandList, rtOutPut, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


			TransitionManagedResource(commandList, blurOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);
			TransitionManagedResource(commandList, depthTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			TransitionManagedResource(commandList, rtIndirectDiffuseOutPut, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			//DenoiseRaytracedSignal(rtIndirectDiffuseOutPut);

			TransitionManagedResource(commandList, rtIndirectSpecularOutPut, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			//DenoiseRaytracedSignal(rtIndirectSpecularOutPut);

			//transition render target to readable texture and then transition it back to render target
			TransitionManagedResource(commandList, taaInput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			TransitionManagedResource(commandList, rtTransparentOutput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			auto rtvHandle = rtCombineOutput.rtvCPUHandle;

			commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
			commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandList->SetGraphicsRootSignature(rtCombineRootSig.Get());
			commandList->SetPipelineState(rtCombinePSO.Get());

			commandList->SetGraphicsRootDescriptorTable(0, rtOutPut.srvGPUHandle);
			commandList->SetGraphicsRootDescriptorTable(1, rtIndirectDiffuseOutPut.srvGPUHandle);
			commandList->SetGraphicsRootDescriptorTable(2, rtIndirectSpecularOutPut.srvGPUHandle);
			commandList->SetGraphicsRootDescriptorTable(3, rtTransparentOutput.srvGPUHandle);


			commandList->DrawInstanced(3, 1, 0, 0);

			//transition render target to readable texture and then transition it back to render target
			TransitionManagedResource(commandList, rtOutPut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);


			TransitionManagedResource(commandList, rtIndirectDiffuseOutPut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			TransitionManagedResource(commandList, rtIndirectSpecularOutPut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			TransitionManagedResource(commandList, rtTransparentOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);


			TransitionManagedResource(commandList, rtCombineOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);
			TransitionManagedResource(commandList, blurOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);

			RenderPostProcessing(rtCombineOutput);

			TransitionManagedResource(commandList, rtCombineOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);
			TransitionManagedResource(commandList, blurOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);

		}


	}
	//RenderEditorWindow();
	TransitionManagedResource(commandList, editorWindowTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	RenderGUI(deltaTime, totalTime);
	TransitionManagedResource(commandList, editorWindowTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);

	transition = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &transition);



	commandList->Close();
	computeCommandList->Close();
	residencySet->Close();

}

void Game::DenoiseRaytracedSignal(ManagedResource& inputTexture)
{
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };

	ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	auto rtvHandle = blurOutput.rtvCPUHandle;

	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootSignature(bilateralRootSig.Get());
	commandList->SetPipelineState(bilateralPSO.Get());

	commandList->SetGraphicsRootDescriptorTable(BilateralBlur::MainColorTex, inputTexture.srvGPUHandle);
	commandList->SetGraphicsRootDescriptorTable(BilateralBlur::DepthTexBlur, depthTex.srvGPUHandle);
	commandList->SetGraphicsRoot32BitConstant(BilateralBlur::BilateralBlurExternalData, renderWidth, 0);
	commandList->SetGraphicsRoot32BitConstant(BilateralBlur::BilateralBlurExternalData, renderHeight, 1);

	commandList->DrawInstanced(3, 1, 0, 0);
}

void Game:: RenderPostProcessing(ManagedResource& inputTexture)
{

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };

	TransitionManagedResource(commandList, velocityBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//TAA
	{
		//applying TAA
		auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
			inputTexture.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &transition);
		inputTexture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaOutput.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &transition);


		ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		auto rtvHandle = taaOutput.rtvCPUHandle;
		TransitionManagedResource(commandList, depthTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(taaRootSig.Get());
		commandList->SetPipelineState(taaPSO.Get());

		commandList->SetGraphicsRootDescriptorTable(0, inputTexture.srvGPUHandle);
		commandList->SetGraphicsRootDescriptorTable(1, taaHistoryBuffer.srvGPUHandle);
		commandList->SetGraphicsRoot32BitConstant(2, numFrames, 0);
		commandList->SetGraphicsRootDescriptorTable(3, velocityBuffer.srvGPUHandle);
		commandList->SetGraphicsRootDescriptorTable(4, depthTex.srvGPUHandle);

		{
			TAAExternData data = {};
			data.inverseProjection = mainCamera->GetInverseProjection();
			data.inverseView = mainCamera->GetInverseView();
			data.prevView = mainCamera->GetViewMatrix();
			data.prevProjection = mainCamera->GetProjectionMatrix();																										

			uint32_t* pDataBegin = 0;
			D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;																							
			dynamicBufferRing.AllocConstantBuffer(sizeof(TAAExternData), reinterpret_cast<void**>(&pDataBegin), &gpuAddress);
			memcpy(pDataBegin, &data, sizeof(TAAExternData));

			commandList->SetGraphicsRootConstantBufferView(5, gpuAddress);
		}

		commandList->DrawInstanced(3, 1, 0, 0);

		//transition render target to readable texture and then transition it back to render target
		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			inputTexture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &transition);
		inputTexture.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaOutput.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &transition);

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaHistoryBuffer.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &transition);

		commandList->CopyResource(taaHistoryBuffer.resource.Get(), taaOutput.resource.Get());

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaHistoryBuffer.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &transition);

		transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaOutput.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &transition);
	}

	//Tonemapping
	{
		//transition render target to readable texture and then transition it back to render target
		auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaOutput.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		commandList->ResourceBarrier(1, &transition);

		TransitionManagedResource(commandList, tonemappingOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);

		ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		auto rtvHandle = tonemappingOutput.rtvCPUHandle;

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(toneMappingRootSig.Get());
		commandList->SetPipelineState(toneMappingPSO.Get());

		commandList->SetGraphicsRootDescriptorTable(1, taaOutput.srvGPUHandle);

		commandList->DrawInstanced(3, 1, 0, 0);

		//transition render target to readable texture and then transition it back to render target
		 transition = CD3DX12_RESOURCE_BARRIER::Transition(
			taaOutput.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_SOURCE);

		commandList->ResourceBarrier(1, &transition);

		TransitionManagedResource(commandList, tonemappingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE);

	}

	D3D12_VIEWPORT viewport1 = {};
	viewport1.Height = static_cast<FLOAT>(height);
	viewport1.Width = static_cast<FLOAT>(width);
	viewport1.MinDepth = 0;
	viewport1.MaxDepth = 1.0f;
	viewport1.TopLeftX = 0;
	viewport1.TopLeftY = 0;

	D3D12_RECT scissorRect1 = {};
	scissorRect1.left = 0;
	scissorRect1.top = 0;
	scissorRect1.right = static_cast<LONG>(width);
	scissorRect1.bottom = static_cast<LONG>(height);

	commandList->RSSetViewports(1, &viewport1);
	commandList->RSSetScissorRects(1, &scissorRect1);
	//Add Fidelity super resolution here
	{
		D3D12_GPU_VIRTUAL_ADDRESS cbHandle = {};
		{
			FSRConstants consts = {};
			FsrEasuCon(reinterpret_cast<AU1*>(&consts.Const0), reinterpret_cast<AU1*>(&consts.Const1), reinterpret_cast<AU1*>(&consts.Const2), 
				reinterpret_cast<AU1*>(&consts.Const3), static_cast<AF1>(renderWidth), static_cast<AF1>(renderHeight), 
				static_cast<AF1>(renderWidth), static_cast<AF1>(renderHeight), (AF1)width, (AF1)height);
			consts.Sample.x = 0;
			uint32_t* pConstMem = 0;
			dynamicBufferRing.AllocConstantBuffer(sizeof(FSRConstants), (void**)&pConstMem, &cbHandle);
			memcpy(pConstMem, &consts, sizeof(FSRConstants));
		}
		// This value is the image region dimension that each thread group of the FSR shader operates on
		static const int threadGroupWorkRegionDim = 16;
		int dispatchX = (width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
		int dispatchY = (height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
		commandList->SetComputeRootSignature(fsrRootSig.Get());

		//EASU pass
		{
			commandList->SetPipelineState(fsrEASUPso.Get());

			//Tonemapping output will be the input texture
			TransitionManagedResource(commandList, tonemappingOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			TransitionManagedResource(commandList, fsrIntermediateTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);


			commandList->SetComputeRootConstantBufferView(0, cbHandle);
			commandList->SetComputeRootDescriptorTable(1, tonemappingOutput.srvGPUHandle);
			commandList->SetComputeRootDescriptorTable(2, fsrIntermediateTexture.uavGPUHandle);

			commandList->Dispatch(dispatchX, dispatchY, 1);

			TransitionManagedResource(commandList, fsrIntermediateTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			TransitionManagedResource(commandList, fsrOutputTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		}

		//RCAS Pass
		{
			FSRConstants consts1 = {};
			FsrRcasCon(reinterpret_cast<AU1*>(&consts1.Const0), 0);
			consts1.Sample.x = 0;
			uint32_t* pConstMem = 0;
			dynamicBufferRing.AllocConstantBuffer(sizeof(FSRConstants), (void**)&pConstMem, &cbHandle);
			memcpy(pConstMem, &consts1, sizeof(FSRConstants));

			commandList->SetPipelineState(fsrRCASPso.Get());

			commandList->SetComputeRootConstantBufferView(0, cbHandle);
			commandList->SetComputeRootDescriptorTable(1, fsrIntermediateTexture.srvGPUHandle);
			commandList->SetComputeRootDescriptorTable(2, fsrOutputTexture.uavGPUHandle);

			commandList->Dispatch(dispatchX, dispatchY, 1);

			TransitionManagedResource(commandList, fsrOutputTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		}
	}

	//fxaa
	{
		//transition render target to readable texture and then transition it back to render target
		TransitionManagedResource(commandList, fxaaOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);


		ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		auto rtvHandle = fxaaOutput.rtvCPUHandle;

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(fxaaRootSig.Get());
		commandList->SetPipelineState(fxaaPSO.Get());

		commandList->SetGraphicsRootDescriptorTable(0, fsrOutputTexture.srvGPUHandle);

		commandList->DrawInstanced(3, 1, 0, 0);

		//transition render target to readable texture and then transition it back to render target
		TransitionManagedResource(commandList, fsrOutputTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);

	}

	//Sharpen pass
	{
		//transition render target to readable texture and then transition it back to render target
		TransitionManagedResource(commandList, fxaaOutput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		
		TransitionManagedResource(commandList, sharpenOutput, D3D12_RESOURCE_STATE_RENDER_TARGET);

		ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		auto rtvHandle = sharpenOutput.rtvCPUHandle;

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(sharpenRootSig.Get());
		commandList->SetPipelineState(sharpenPSO.Get());

		commandList->SetGraphicsRootDescriptorTable(0, fxaaOutput.srvGPUHandle);

		commandList->DrawInstanced(3, 1, 0, 0);

	}

	//fullscreen pass through
	{
	
		TransitionManagedResource(commandList, sharpenOutput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
		ID3D12DescriptorHeap* ppHeaps[] = { renderTargetSRVHeap.GetHeapPtr() };
	
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	
		auto rtvHandle = rtvDescriptorHeap.GetCPUHandle(frameIndex);

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
		commandList->SetGraphicsRootSignature(passthroughRootSig.Get());
		commandList->SetPipelineState(passthroughPSO.Get());
	
		commandList->SetGraphicsRootDescriptorTable(0, sharpenOutput.srvGPUHandle);
	
		commandList->DrawInstanced(3, 1, 0, 0);
	
		TransitionManagedResource(commandList, sharpenOutput, D3D12_RESOURCE_STATE_COPY_SOURCE);

	}

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
	desc.Height = renderHeight;
	desc.Width = renderWidth;
	desc.Depth = 1;

	commandList->SetPipelineState1(gbufferStateObject.Get());
	commandList->DispatchRays(&desc);
}

void Game::CreateDirectRays()
{
	//creating a dispatch rays description
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//raygeneration location
	desc.RayGenerationShaderRecord.StartAddress = sbtResource->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
	desc.RayGenerationShaderRecord.SizeInBytes = sbtGenerator.GetRayGenSectionSize();

	//miss shaders
	desc.MissShaderTable.StartAddress = desc.RayGenerationShaderRecord.StartAddress + sbtGenerator.GetRayGenSectionSize();
	desc.MissShaderTable.SizeInBytes = sbtGenerator.GetMissSectionSize();
	desc.MissShaderTable.StartAddress = AlignUp(desc.MissShaderTable.StartAddress, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	desc.MissShaderTable.StrideInBytes = AlignUp(sbtGenerator.GetMissEntrySize(), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	//hit groups
	desc.HitGroupTable.StartAddress = desc.MissShaderTable.StartAddress + sbtGenerator.GetMissSectionSize();
	//desc.HitGroupTable.StartAddress = AlignUp(desc.HitGroupTable.StartAddress, 1);
	desc.HitGroupTable.SizeInBytes = sbtGenerator.GetHitGroupSectionSize();
	desc.HitGroupTable.StrideInBytes = AlignUp(sbtGenerator.GetHitGroupEntrySize(), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	//scene description
	desc.Height = renderHeight;
	desc.Width = renderWidth;
	desc.Depth = 1;

	commandList->SetPipelineState1(rtStateObject.Get());
	commandList->DispatchRays(&desc);

	WaitForPreviousFrame();
	CopyResource(commandList, currentReservoir, intermediateReservoir);

	//Restrir spatial reuse pass goes here
	if (doRestir && restirSpatialReuse)
	{
		commandList->SetComputeRootSignature(restirSpatialReuseRootSig.Get());
		commandList->SetPipelineState(restirSpatialReusePSO.Get());

		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferDif, rtAlbedo.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferNorm, rtNormals.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferRoughMetal, rtRoughnessMetal.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferPos, rtPosition.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_OutColor, rtOutPut.uavGPUHandle);

		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_Reservoirs, intermediateReservoir.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_OutReservoirs, currentReservoir.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_SampleSequences, retargetedSequences.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootShaderResourceView(RestrirSpatialReuseIndices::RestirSpatialReuse_Lights, lightListResource->GetGPUVirtualAddress());

		auto camPos = mainCamera->GetPosition();

		commandList->SetComputeRoot32BitConstants(RestrirSpatialReuseIndices::RestirSpatialReuse_ExternData, 3, &camPos, 0);

		auto dispatchX = renderWidth / 16;
		auto dispatchY = renderHeight / 16;

		commandList->Dispatch(dispatchX, dispatchY, 1);

		//CopyResource(commandList, currentReservoir, intermediateReservoir);

	}

	//else
		//CopyResource(commandList, currentReservoir, intermediateReservoir);
}

void Game::CreateIndirectDiffuseRays()
{
	//creating a dispatch rays description
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//raygeneration location
	desc.RayGenerationShaderRecord.StartAddress = indirectDiffuseSbtResource->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
	desc.RayGenerationShaderRecord.SizeInBytes = indirectDiffuseSbtGenerator.GetRayGenSectionSize();

	//miss shaders
	desc.MissShaderTable.StartAddress = indirectDiffuseSbtResource->GetGPUVirtualAddress() + indirectDiffuseSbtGenerator.GetRayGenSectionSize();
	desc.MissShaderTable.SizeInBytes = indirectDiffuseSbtGenerator.GetMissSectionSize();
	//desc.MissShaderTable.StartAddress = AlignUp(desc.MissShaderTable.StartAddress, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	desc.MissShaderTable.StrideInBytes = indirectDiffuseSbtGenerator.GetMissEntrySize();

	//hit groups
	desc.HitGroupTable.StartAddress = indirectDiffuseSbtResource->GetGPUVirtualAddress() + indirectDiffuseSbtGenerator.GetRayGenSectionSize() + indirectDiffuseSbtGenerator.GetMissSectionSize();
	//desc.HitGroupTable.StartAddress = (desc.HitGroupTable.StartAddress + 63) & ~63;
	desc.HitGroupTable.SizeInBytes = indirectDiffuseSbtGenerator.GetHitGroupSectionSize();
	desc.HitGroupTable.StrideInBytes = indirectDiffuseSbtGenerator.GetHitGroupEntrySize();

	//scene description
	desc.Height = renderHeight;
	desc.Width = renderWidth;
	desc.Depth = 1;

	commandList->SetPipelineState1(indirectDiffuseRtStateObject.Get());
	commandList->DispatchRays(&desc);
	WaitForPreviousFrame();

	//Restrir spatial reuse pass goes here
	if (doRestirGI && restirSpatialReuse)
	{
		commandList->SetComputeRootSignature(restirSpatialReuseRootSig.Get());
		commandList->SetPipelineState(restirGISpatialReusePSO.Get());

		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferDif, rtAlbedo.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferNorm, rtNormals.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferRoughMetal, rtRoughnessMetal.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_GBufferPos, rtPosition.uavGPUHandle);
		commandList->SetComputeRootDescriptorTable(RestrirSpatialReuseIndices::RestirSpatialReuse_OutColor, rtIndirectDiffuseOutPut.uavGPUHandle);

		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_Reservoirs, indirectDiffuseTemporalReservoir.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_OutReservoirs, indirectDiffuseSpatialReservoir.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootUnorderedAccessView(RestrirSpatialReuseIndices::RestirSpatialReuse_SampleSequences, retargetedSequences.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootShaderResourceView(RestrirSpatialReuseIndices::RestirSpatialReuse_Lights, lightListResource->GetGPUVirtualAddress());
		commandList->SetComputeRootShaderResourceView(RestrirSpatialReuseIndices::RestirSpatialReuse_AccelStruct, topLevelAsBuffers.pResult->GetGPUVirtualAddress());

		auto camPos = mainCamera->GetPosition();

		commandList->SetComputeRoot32BitConstants(RestrirSpatialReuseIndices::RestirSpatialReuse_ExternData, 3, &camPos, 0);

		auto dispatchX = renderWidth / 16;
		auto dispatchY = renderHeight / 16;

		commandList->Dispatch(dispatchX, dispatchY, 1);
	}
}

void Game::CreateIndirectSpecularRays()
{
	//creating a dispatch rays description
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//raygeneration location
	desc.RayGenerationShaderRecord.StartAddress = indirectSpecularSbtResource->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
	desc.RayGenerationShaderRecord.SizeInBytes = indirectSpecularSbtGenerator.GetRayGenSectionSize();

	//miss shaders
	desc.MissShaderTable.StartAddress = indirectSpecularSbtResource->GetGPUVirtualAddress() + indirectSpecularSbtGenerator.GetRayGenSectionSize();
	desc.MissShaderTable.SizeInBytes = indirectSpecularSbtGenerator.GetMissSectionSize();
	//desc.MissShaderTable.StartAddress = (desc.MissShaderTable.StartAddress + 63) & ~63;
	desc.MissShaderTable.StrideInBytes = indirectSpecularSbtGenerator.GetMissEntrySize();

	//hit groups
	desc.HitGroupTable.StartAddress = indirectSpecularSbtResource->GetGPUVirtualAddress() + indirectSpecularSbtGenerator.GetRayGenSectionSize() + indirectSpecularSbtGenerator.GetMissSectionSize();
	//desc.HitGroupTable.StartAddress = (desc.HitGroupTable.StartAddress + 63) & ~63;
	desc.HitGroupTable.SizeInBytes = indirectSpecularSbtGenerator.GetHitGroupSectionSize();
	desc.HitGroupTable.StrideInBytes = indirectSpecularSbtGenerator.GetHitGroupEntrySize();

	//scene description
	desc.Height = renderHeight;
	desc.Width = renderWidth;
	desc.Depth = 1;

	commandList->SetPipelineState1(indirectSpecularRtStateObject.Get());
	commandList->DispatchRays(&desc);
}

void Game::CreateTransparencyRays()
{
	//creating a dispatch rays description
	D3D12_DISPATCH_RAYS_DESC desc = {};
	//raygeneration location
	desc.RayGenerationShaderRecord.StartAddress = transparentSbtResource->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.StartAddress = (desc.RayGenerationShaderRecord.StartAddress + 63) & ~63;
	desc.RayGenerationShaderRecord.SizeInBytes = transparentSbtGenerator.GetRayGenSectionSize();

	//miss shaders
	desc.MissShaderTable.StartAddress = transparentSbtResource->GetGPUVirtualAddress() + transparentSbtGenerator.GetRayGenSectionSize();
	desc.MissShaderTable.SizeInBytes = transparentSbtGenerator.GetMissSectionSize();
	//desc.MissShaderTable.StartAddress = (desc.MissShaderTable.StartAddress + 63) & ~63;
	desc.MissShaderTable.StrideInBytes = transparentSbtGenerator.GetMissEntrySize();

	//hit groups
	desc.HitGroupTable.StartAddress = transparentSbtResource->GetGPUVirtualAddress() + transparentSbtGenerator.GetRayGenSectionSize() + transparentSbtGenerator.GetMissSectionSize();
	//desc.HitGroupTable.StartAddress = (desc.HitGroupTable.StartAddress + 63) & ~63;
	desc.HitGroupTable.SizeInBytes = transparentSbtGenerator.GetHitGroupSectionSize();
	desc.HitGroupTable.StrideInBytes = transparentSbtGenerator.GetHitGroupEntrySize();

	//scene description
	desc.Height = renderHeight;
	desc.Width = renderWidth;
	desc.Depth = 1;

	commandList->SetPipelineState1(rtTransparentStateObject.Get());
	commandList->DispatchRays(&desc);
}

void Game::CreateLTCTexture()
{
	ltcDescriptorHeap.Create( 3 + 1 + 100, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ltcTempDescriptorHeap.Create( 3 + 1 + 100, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ltcDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/ltc_1.png", ltcLUT, RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	ltcDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/ltc_2.png", ltcLUT2, RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_0.png", ltcTexture[0], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[0].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_1.png", ltcTexture[1], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[1].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_2.png", ltcTexture[2], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[2].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_3.png", ltcTexture[3], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[3].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_4.png", ltcTexture[4], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[4].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_5.png", ltcTexture[5], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[5].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_6.png", ltcTexture[6], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[6].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);
	ltcTempDescriptorHeap.CreateDescriptor(L"../../Assets/Textures/Brick_7.png", ltcTexture[7], RESOURCE_TYPE_SRV,   TEXTURE_TYPE_DEAULT, false);
	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcTexture[7].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	auto desc = ltcTexture[0].resource->GetDesc();
	//creating the texture cube array
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Format = desc.Format;
	resourceDesc.MipLevels = desc.MipLevels;
	resourceDesc.DepthOrArraySize = 8;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Width = desc.Width;
	resourceDesc.Height = desc.Height;


	ThrowIfFailed
	(
		GetAppResources().device->CreateCommittedResource
		(
			&GetAppResources().defaultHeapType,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(ltcPrefilterTexture.resource.GetAddressOf())
		)
	);

	ltcPrefilterTexture.resource->SetName(L"ltcprefiltertexture");

	D3D12_BOX box = {};
	box.top = 0;
	box.left = 0;
	box.bottom = desc.Height;
	box.right = desc.Width;
	box.front = 0;
	box.back = 1;

	for (size_t i = 0; i < 8; i++)
	{
		// Copy
		D3D12_TEXTURE_COPY_LOCATION destLoc = {};
		destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destLoc.pResource = ltcPrefilterTexture.resource.Get();
		destLoc.SubresourceIndex = D3D12CalcSubresource(0, i, 0, desc.MipLevels, 0);

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLoc.pResource = ltcTexture[i].resource.Get();
		srcLoc.SubresourceIndex = D3D12CalcSubresource(0, 0, 0, desc.MipLevels, 0);

		commandList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, &box);
	}

	 transition = CD3DX12_RESOURCE_BARRIER::Transition(ltcPrefilterTexture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);

	ltcDescriptorHeap.CreateDescriptor(ltcPrefilterTexture, RESOURCE_TYPE_SRV, 0, 0, 0, 0, desc.MipLevels, true);


	if (gpuHeapRingBuffer != nullptr)
	{
		gpuHeapRingBuffer->AllocateStaticDescriptors(3, ltcDescriptorHeap);
		ltcLUT.heapOffset = gpuHeapRingBuffer->GetNumStaticResources() - 3;
	}
}

void Game::PrefilterLTCTextures()
{

}


void Game::WaitForPreviousFrame()
{
	//signal and increment the fence
	//const UINT64 pfence = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValues[frameIndex]));
	//fenceValue++;

	//wait until the previous frame is finished
	//if (fence->GetCompletedValue() < pfence)
	//{
	ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
	WaitForSingleObjectEx(fenceEvent, INFINITE, false);
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


	ThrowIfFailed(commandAllocators[frameIndex]->Reset());
	ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

	ThrowIfFailed(computeCommandAllocator[frameIndex]->Reset());
	ThrowIfFailed(computeCommandList->Reset(computeCommandAllocator[frameIndex].Get(), computePipelineState.Get()));
}

void Game::UploadTextureToRingBuffer(std::wstring filename, ManagedResource& resource)
{
	DescriptorHeapWrapper dummyWrapper;
	dummyWrapper.Create(1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dummyWrapper.CreateDescriptor(filename, resource, RESOURCE_TYPE_SRV, TEXTURE_TYPE_DEAULT);

	if (gpuHeapRingBuffer != nullptr)
	{
		gpuHeapRingBuffer->AllocateStaticDescriptors(1, dummyWrapper);
		resource.heapOffset = gpuHeapRingBuffer->GetNumStaticResources() - 1;
	}
	
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


	if (buttonState & 0x0002)
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