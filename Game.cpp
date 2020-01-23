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


	ComPtr<ID3DBlob> vertexShaderBlob;
	ComPtr<ID3DBlob> pixelShaderBlob;

	frameIndex = this->swapChain->GetCurrentBackBufferIndex();

	HRESULT hr;

	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer) + 255) & ~255;
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
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 2;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(constantBufferHeap.GetAddressOf()));
		if (FAILED(hr)) return hr;

		cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

	//this describes the type of constant buffer and which register to map the data to
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	CD3DX12_ROOT_PARAMETER1 rootParams[1]; // specifies the descriptor table
	//ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	//rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

	rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
		signature.GetAddressOf(), error.GetAddressOf());
	if (FAILED(hr)) return hr;

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf()));

	if (FAILED(hr)) return hr;

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
	// Create some temporary variables to represent colors
	// - Not necessary, just makes things more readable
	XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	XMFLOAT4 blue = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);


	/*Vertex triangleVBO[] =
	{
		{ { +0.0f, +1.0f, +0.0f }, { 1.0f, 0.0f, 0.0f},{1.0,0.f} },
		{ { +1.5f, -1.0f, +0.0f }, { 0.0f, 1.0f, 0.0f},{1.0,0.f}  },
		{ { -1.5f, -1.0f, +0.0f }, { 0.0f, 0.0f, 1.0f},{1.0,0.f}  }
	};

	unsigned int indexListMesh1[] = { 0,1,2 };

	UINT vertexBufferSize = sizeof(triangleVBO);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(vertexBuffer.GetAddressOf())
	));

	ComPtr<ID3D12Resource> vbufferUpload;
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vbufferUpload.GetAddressOf())
	));
	D3D12_SUBRESOURCE_DATA bufferData = {};
	bufferData.pData = reinterpret_cast<BYTE*>(triangleVBO);
	bufferData.RowPitch = vertexBufferSize;
	bufferData.SlicePitch = vertexBufferSize;

	UpdateSubresources<1>(commandList.Get(), vertexBuffer.Get(), vbufferUpload.Get(), 0, 0, 1,&bufferData);
	//copy triangle data to vertex buffer
	UINT8* vertexDataBegin;
	//ThrowIfFailed(vbufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
	//memcpy(vertexDataBegin, triangleVBO, sizeof(triangleVBO));
	//vbufferUpload->Unmap(0, nullptr);

	///commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		//D3D12_RESOURCE_STATE_COPY_DEST));
	//commandList->CopyResource(vertexBuffer.Get(), vbufferUpload.Get());*
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	//command lists are created in record state but since there is nothing to record yet
	//close it for the main loop

	//ThrowIfFailed(commandList->Close());
	//ID3D12CommandList* commandLists[] = { commandList.Get() };
	//commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	//WaitForPreviousFrame();

	//vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	//vertexBufferView.StrideInBytes = sizeof(Vertex);
	//vertexBufferView.SizeInBytes = sizeof(triangleVBO);*/

	//mesh1 = std::make_shared<Mesh>(triangleVBO, 3, indexListMesh1, _countof(indexListMesh1), device, commandList);
	//mesh2 = std::make_shared<Mesh>(triangleVBO, 3, indexListMesh1, _countof(indexListMesh1), device, commandList, commandQueue, this);

		//creating the vertex buffer
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	float aspectRatio = static_cast<float>(width / height);

	mesh1 = std::make_shared<Mesh>("../../Assets/Models/sphere.obj", device, commandList);
	mesh2 = std::make_shared<Mesh>("../../Assets/Models/shark.obj", device, commandList);
	std::shared_ptr<Mesh> mesh3 = std::make_shared<Mesh>("../../Assets/Models/helix.obj", device, commandList);
	entity1 = std::make_shared<Entity>(mesh1);
	entity2 = std::make_shared<Entity>(mesh2);
	entity1->SetPosition(XMFLOAT3(0, 0, 1.5f));
	entity2->SetPosition(XMFLOAT3(3, 0, 1.0f));

	entities.emplace_back(entity1);
	entities.emplace_back(entity2);
	entities.emplace_back(std::make_shared<Entity>(mesh3));
	entities[2]->SetPosition(XMFLOAT3(-1.5f, 0, 1.5f));

	//copying the data from upload heaps to default heaps
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	//creating the constant buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),//must be a multiple of 64kb
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constantBuffer.GetAddressOf())
	));

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

	ZeroMemory(&constantBufferData, sizeof(constantBufferData));

	//setting range to 0,0 so that the cpu cannot read from this resource
	//can keep the constant buffer mapped for the entire application
	ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&constantBufferBegin)));
	for (int i = 0; i < entities.size(); i++)
	{
		memcpy(constantBufferBegin + (i * sceneConstantBufferAlignmentSize), &constantBufferData, sizeof(constantBufferData));
	}




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

	//constantBufferData.offset.x += translationSpeed;
	/*constantBufferData.world = entity1->GetModelMatrix();

	memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
	constantBufferData.world = entity2->GetModelMatrix();
	memcpy(constantBufferBegin + sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));*/


	for (size_t i = 0; i < entities.size(); i++)
	{
		constantBufferData.world = entities[i]->GetModelMatrix();
		memcpy(constantBufferBegin + (i * (size_t)sceneConstantBufferAlignmentSize), &constantBufferData, sizeof(constantBufferData));
		//std::copy(&constantBufferData, &constantBufferData + sizeof(constantBufferData), &constantBufferBegin);
	}

}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	/*context->ClearRenderTargetView(backBufferRTV, color);
	context->ClearDepthStencilView(
		depthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	// Send data to shader variables
	//  - Do this ONCE PER OBJECT you're drawing
	//  - This is actually a complex process of copying data to a local buffer
	//    and then copying that entire buffer to the GPU.  
	//  - The "SimpleShader" class handles all of that for you.
	vertexShader->SetMatrix4x4("world", worldMatrix);
	vertexShader->SetMatrix4x4("view", viewMatrix);
	vertexShader->SetMatrix4x4("projection", projectionMatrix);

	// Once you've set all of the data you care to change for
	// the next draw call, you need to actually send it to the GPU
	//  - If you skip this, the "SetMatrix" calls above won't make it to the GPU!
	vertexShader->CopyAllBufferData();

	// Set the vertex and pixel shaders to use for the next Draw() command
	//  - These don't technically need to be set every frame...YET
	//  - Once you start applying different shaders to different objects,
	//    you'll need to swap the current shaders before each draw
	vertexShader->SetShader();
	pixelShader->SetShader();

	//drawing the three meshes to the scene
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	//first mesh

	//setting the vertex and index buffer
	auto tempVertexBuffer = mesh1->GetVertexBuffer();
	context->IASetVertexBuffers(0, 1, &tempVertexBuffer, &stride, &offset);
	auto tempIndexBuffer = mesh1->GetIndexBuffer();
	context->IASetIndexBuffer(mesh1->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);

	//drawing the first mesh
	context->DrawIndexed(mesh1->GetIndexCount(), 0, 0);

	//drawing the second mesh

	//setting the index and vertex buffer
	tempVertexBuffer = mesh2->GetVertexBuffer();
	context->IASetVertexBuffers(0, 1, &tempVertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(mesh2->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
	context->DrawIndexed(mesh2->GetIndexCount(), 0, 0);

	//drawing the third mesh
	tempVertexBuffer = mesh3->GetVertexBuffer();
	context->IASetVertexBuffers(0, 1, &tempVertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(mesh3->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
	context->DrawIndexed(mesh3->GetIndexCount(), 0, 0);

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain effect,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, &backBufferRTV, depthStencilView);*/
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
	ID3D12DescriptorHeap* ppHeaps[] = { constantBufferHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//set the descriptor table 0 as the constant buffer descriptor
	
	//commandList->SetGraphicsRootDescriptorTable(0, constantBufferHeap->GetGPUDescriptorHandleForHeapStart());
	//commandList->SetGraphic
	//commandList-

	commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());

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



	/**/for (UINT64 i = 0; i < entities.size(); i++)
	{
		D3D12_VERTEX_BUFFER_VIEW vertexBuffer = entities[i]->GetMesh()->GetVertexBuffer();
		auto indexBuffer = entities[i]->GetMesh()->GetIndexBuffer();
		commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress() + sceneConstantBufferAlignmentSize * i);
		commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
		commandList->IASetIndexBuffer(&indexBuffer);
		commandList->DrawIndexedInstanced(entities[i]->GetMesh()->GetIndexCount(), 1, 0, 0, 0); 

	}

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