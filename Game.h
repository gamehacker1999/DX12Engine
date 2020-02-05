#pragma once
#include "DXCore.h"
#include "Camera.h"
#include"Mesh.h"
#include"Entity.h"
#include"Lights.h"
#include"DescriptorHeapWrapper.h"
#include"CommonStructs.h"
#include"Material.h"
#include"Skybox.h"
#include"GPUHeapRingBuffer.h"

#include <DirectXMath.h>

struct LightData
{
	DirectionalLight light1;
	XMFLOAT3 cameraPosition;
};

class Game
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	HRESULT Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void PopulateCommandList();
	void WaitForPreviousFrame();
	void MoveToNextFrame();

	// Overridden mouse input helper methods
	void OnMouseDown(WPARAM buttonState, int x, int y);
	void OnMouseUp(WPARAM buttonState, int x, int y);
	void OnMouseMove(WPARAM buttonState, int x, int y);
	void OnMouseWheel(float wheelDelta, int x, int y);
private:

	int sceneConstantBufferAlignmentSize;

	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadShaders();
	void CreateMatrices();
	void CreateBasicGeometry();
	void CreateEnvironment();

	//app resources
	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	ComPtr<ID3D12GraphicsCommandList> skyboxBundle;
	ComPtr<ID3D12CommandAllocator> bundleAllocator;

	// The matrices to go from model space to screen space
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;

	//ComPtr<ID3D12DescriptorHeap> mainBufferHeap;
	DescriptorHeapWrapper mainBufferHeap;
	ComPtr<ID3D12Resource> constantBufferResource;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mainCPUDescriptorHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mainGPUDescriptorHandle;
	ComPtr<ID3D12Resource> cbufferUploadHeap;
	SceneConstantBuffer constantBufferData;
	UINT cbvDescriptorSize;
	UINT8* constantBufferBegin;

	ComPtr<ID3D12Resource> lightConstantBufferResource;
	UINT8* lightCbufferBegin;
	LightData lightData;

	//ComPtr<ID3D12Resource> depthStencilBuffer;
	ManagedResource depthStencilBuffer;
	//ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;
	DescriptorHeapWrapper dsDescriptorHeap;

	std::shared_ptr<Camera> mainCamera;

	std::shared_ptr<Mesh> mesh1;
	std::shared_ptr<Entity> entity1;
	std::shared_ptr<Mesh> mesh2;
	std::shared_ptr<Mesh> mesh3;
	std::shared_ptr<Entity> entity2;
	std::shared_ptr<Entity> entity3;
	std::shared_ptr<Entity> entity4;
	std::shared_ptr<Entity> entity5;
	std::shared_ptr<Material> material1;
	std::shared_ptr<Material> material2;

	std::vector<std::shared_ptr<Entity>> entities;
	std::vector<std::shared_ptr<Material>> materials;

	//environment variables
	ComPtr<ID3D12RootSignature> skyboxRootSignature;
	ComPtr<ID3D12PipelineState> skyboxPSO;
	std::shared_ptr<Skybox> skybox;

	//ring buffer
	std::shared_ptr<GPUHeapRingBuffer> gpuHeapRingBuffer;

	//pbr pipeline state
	ComPtr<ID3D12PipelineState> pbrPipelineState;

	//managing the residency
	D3DX12Residency::ResidencyManager residencyManager;
	D3DX12Residency::ResidencySet residencySet;

	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;
};

