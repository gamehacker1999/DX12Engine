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
	lightCbufferBegin = 0;

}

Game::~Game()
{
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
HRESULT Game::Init()
{

	frameIndex = this->swapChain->GetCurrentBackBufferIndex();

	HRESULT hr;

	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer));
	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = frameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = (device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap)));
		if (FAILED(hr)) return hr;

		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		//creating a srv,uav, cbv descriptor heap
		/*D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 4+1+1;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(mainBufferHeap.GetAddressOf()));
		if (FAILED(hr)) return hr;

		mainCPUDescriptorHandle = mainBufferHeap->GetCPUDescriptorHandleForHeapStart();
		mainGPUDescriptorHandle = mainBufferHeap->GetGPUDescriptorHandleForHeapStart();

		cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);*/

		ThrowIfFailed(mainBufferHeap.Create(device, 4 + 1 + 1, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		//creating the depth stencil heap
		D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
		dsHeapDesc.NumDescriptors = 1;
		dsHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&dsHeapDesc, IID_PPV_ARGS(dsDescriptorHeap.GetAddressOf()));
		if (FAILED(hr)) return hr;


	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < frameCount; n++)
		{
			hr = (this->swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
			if (FAILED(hr)) return hr;
			device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtvDescriptorSize);
		}

	}

	//creating depth stencil view
	D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
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
		IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())
	));

	device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	hr = (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
	if (FAILED(hr)) return hr;

	//create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(),
		IID_PPV_ARGS(commandList.GetAddressOf())));

	//memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	//memcpy(constantBufferBegin+sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));

	
	//create synchronization object and wait till the objects have been passed to the gpu
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	fenceValue = 1;
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

	mainCamera = std::make_shared<Camera>(XMFLOAT3(0.0f, 0.f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f));

	mainCamera->CreateProjectionMatrix((float)width / height); //creating the camera projection matrix


	WaitForPreviousFrame();

	return S_OK;
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files 
// --------------------------------------------------------
void Game::LoadShaders()
{

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
	lightData.light1.direction = XMFLOAT3(0, 0, 1);
	lightData.light1.ambientColor = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
	lightData.light1.specularity = XMFLOAT4(1, 0, 0, 1);

	lightConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightCbufferBegin));
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));


	UINT64 cbufferOffset = 0;
	mesh1 = std::make_shared<Mesh>("../../Assets/Models/sphere.obj", device, commandList);
	mesh2 = std::make_shared<Mesh>("../../Assets/Models/cube.obj", device, commandList);
	mesh3 = std::make_shared<Mesh>("../../Assets/Models/helix.obj", device, commandList);
	

		//this describes the type of constant buffer and which register to map the data to
	CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
	CD3DX12_ROOT_PARAMETER1 rootParams[4]; // specifies the descriptor table
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[1].InitAsConstants(1, 0, 0,D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[2].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC,D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	
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
	//load shaders
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.GetAddressOf()));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.GetAddressOf()));

	//input vertex layout, describes the semantics

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

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

	//creating the vertex buffer
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	float aspectRatio = static_cast<float>(width / height);

	//CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mainCPUDescriptorHandle, 0, cbvDescriptorSize);
	material1 = std::make_shared<Material>(L"../../Assets/Textures/Brick.jpg", device, commandQueue,mainBufferHeap);
	entity1 = std::make_shared<Entity>(mesh1,material1);
	entity2 = std::make_shared<Entity>(mesh1,material1);
	entity3 = std::make_shared<Entity>(mesh1,material1);
	entity4 = std::make_shared<Entity>(mesh3,material1);
	

	entity1->SetPosition(XMFLOAT3(0, 0, 1.5f));
	entity2->SetPosition(XMFLOAT3(1, 0, 1.0f));
	entity3->SetPosition(XMFLOAT3(-1, 0, 1.f));
	entity4->SetPosition(XMFLOAT3(-4, 0, 1.f));


	entity1->PrepareConstantBuffers(mainBufferHeap, device);
	entity2->PrepareConstantBuffers(mainBufferHeap, device);
	entity3->PrepareConstantBuffers(mainBufferHeap, device);
	entity4->PrepareConstantBuffers(mainBufferHeap, device);


	entities.emplace_back(entity1);
	entities.emplace_back(entity2);
	entities.emplace_back(entity3);
	entities.emplace_back(entity4);


	//copying the data from upload heaps to default heaps
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

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

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();

	mainCamera->Update(deltaTime);

	const float translationSpeed = 0.005f;
	const float offsetBounds = 1.25f;


	constantBufferData.projection = mainCamera->GetProjectionMatrix();
	constantBufferData.view = mainCamera->GetViewMatrix();

	lightData.cameraPosition = mainCamera->GetPosition();
	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));

	//constantBufferData.offset.x += translationSpeed;
	/*constantBufferData.world = entity1->GetModelMatrix();

	memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	constantBufferData.world = entity2->GetModelMatrix();
	memcpy(constantBufferBegin + sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));*/


	/*for (size_t i = 0; i < entities.size(); i++)
	{
		constantBufferData.world = entities[i]->GetModelMatrix();
		memcpy(constantBufferBegin + (i * (size_t)sceneConstantBufferAlignmentSize), &constantBufferData, sizeof(constantBufferData));
		//std::copy(&constantBufferData, &constantBufferData + sizeof(constantBufferData), &constantBufferBegin);
	}*/

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
	commandQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);

	//present the frame
	ThrowIfFailed(swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void Game::PopulateCommandList()
{
	ThrowIfFailed(commandAllocator->Reset());
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState.Get()));

	//set necessary state
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//setting the constant buffer descriptor table
	ID3D12DescriptorHeap* ppHeaps[] = { mainBufferHeap.GetHeap().Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//set the descriptor table 0 as the constant buffer descriptor
	
	//commandList->SetGraphicsRootDescriptorTable(0, constantBufferHeap->GetGPUDescriptorHandleForHeapStart());
	//commandList->SetGraphic
	//commandList-

	//commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource->GetGPUVirtualAddress());

	//indicate that the back buffer is the render target
	commandList->ResourceBarrier(1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		frameIndex,rtvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &CD3DX12_CPU_DESCRIPTOR_HANDLE(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart()));
	commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
/**/


	//record commands
	const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	/*D3D12_VERTEX_BUFFER_VIEW vertexBuffer = entities[0]->GetMesh()->GetVertexBuffer();
	commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
	auto indexBuffer = entities[0]->GetMesh()->GetIndexBuffer();
	commandList->IASetIndexBuffer(&indexBuffer);
	unsigned int indexCount = entities[0]->GetMesh()->GetIndexCount();
	//commandList->DrawInstanced(3, 1, 0, 0);
	commandList->DrawIndexedInstanced(indexCount,1,0,0,0);

	//constantBufferData.world = entity2->GetModelMatrix();
	//memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress()+sceneConstantBufferAlignmentSize);
	commandList->IASetVertexBuffers(0, 1, &entities[1]->GetMesh()->GetVertexBuffer());
	commandList->IASetIndexBuffer(&entities[1]->GetMesh()->GetIndexBuffer());
	commandList->DrawIndexedInstanced(entities[1]->GetMesh()->GetIndexCount(), 1, 0, 0, 0);*/


	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCBVSRVUAVHandle(mainBufferHeap->GetGPUDescriptorHandleForHeapStart(),0,cbvDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(3, gpuCBVSRVUAVHandle);
	gpuCBVSRVUAVHandle.Offset(cbvDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(0, gpuCBVSRVUAVHandle);

	/**/for (UINT i = 0; i < entities.size(); i++)
	{
		entities[i]->PrepareMaterial(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix());
		commandList->SetGraphicsRoot32BitConstant(1, i, 0);
		D3D12_VERTEX_BUFFER_VIEW vertexBuffer = entities[i]->GetMesh()->GetVertexBuffer();
		auto indexBuffer = entities[i]->GetMesh()->GetIndexBuffer();
		commandList->SetGraphicsRootConstantBufferView(2, lightConstantBufferResource->GetGPUVirtualAddress());
		//gpuCBVSRVUAVHandle.Offset(cbvDescriptorSize);
		//commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource->GetGPUVirtualAddress() + sceneConstantBufferAlignmentSize * i);
		commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
		commandList->IASetIndexBuffer(&indexBuffer);
		commandList->DrawIndexedInstanced(entities[i]->GetMesh()->GetIndexCount(), 1, 0, 0, 0); 

	}

	//drawing the skybox
	gpuCBVSRVUAVHandle.Offset((INT)entities.size() * cbvDescriptorSize);

	skybox->PrepareForDraw(mainCamera->GetViewMatrix(), mainCamera->GetProjectionMatrix(), mainCamera->GetPosition());
	commandList->SetPipelineState(skybox->GetPipelineState().Get());
	commandList->SetGraphicsRootSignature(skybox->GetRootSignature().Get());
	commandList->SetGraphicsRootConstantBufferView(0, skybox->GetConstantBuffer()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, gpuCBVSRVUAVHandle);
	commandList->IASetVertexBuffers(0, 1, &skybox->GetMesh()->GetVertexBuffer());
	commandList->IASetIndexBuffer(&skybox->GetMesh()->GetIndexBuffer());
	commandList->DrawIndexedInstanced(skybox->GetMesh()->GetIndexCount(), 1, 0, 0, 0);

	//back buffer will now be used to present

	// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), 
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	commandList->Close();
}

void Game::WaitForPreviousFrame()
{
	//signal and increment the fence
	const UINT64 pfence = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(),pfence));
	fenceValue++;

	//wait until the previous frame is finished
	if (fence->GetCompletedValue() < pfence)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(pfence, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}
	
	//WaitToFlushGPU(commandQueue,fence,fenceValue,fenceEvent);
	frameIndex = swapChain->GetCurrentBackBufferIndex();
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