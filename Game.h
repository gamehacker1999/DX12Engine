#pragma once
#include "DXCore.h"
#include <DXRHelper.h>
#include "Camera.h"
#include"Mesh.h"
#include"Entity.h"
#include"Emitter.h"
#include"Lights.h"
#include"DescriptorHeapWrapper.h"
#include"CommonStructs.h"
#include"Material.h"
#include"Skybox.h"
#include"GPUHeapRingBuffer.h"
#include"RaymarchedVolume.h"

#include"Velocity.h"

#include <DirectXMath.h>
#define ENTT_STANDARD_CPP
#include<entity\registry.hpp>
#include"Flocker.h"

struct LightData
{
	DirectionalLight light1;
	XMFLOAT3 cameraPosition;
};

struct RayTraceCameraData
{
	XMFLOAT4X4 view;
	XMFLOAT4X4 proj;
	XMFLOAT4X4 iView;
	XMFLOAT4X4 iProj;
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

	//------------------Raytracing Functions--------------------------

	//create the acceleration structure for the buffers
	AccelerationStructureBuffers CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>,uint32_t>> vertexBuffers);
	//create top level acceleration structures
	void CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>,XMMATRIX>>& instances);
	//create both top and bottom structures
	void CreateAccelerationStructures();

	//rootgeneration functions
	ComPtr<ID3D12RootSignature> CreateRayGenRootSignature();
	ComPtr<ID3D12RootSignature> CreateMissRootSignature();
	ComPtr<ID3D12RootSignature> CreateClosestHitRootSignature();

	//creating the output texture and the descriptor heap for dxr
	void CreateRaytracingOutputBuffer();
	void CreateRaytracingDescriptorHeap();

	//create dxr pipeline
	void CreateRayTracingPipeline();

	//shader binding table
	void CreateShaderBindingTable();

	//----------------------------------------------------------------

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
	std::shared_ptr<Mesh> sharkMesh;
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
	std::shared_ptr<D3DX12Residency::ResidencySet> residencySet;

	ComPtr<ID3D12RootSignature> volumeRootSignature;
	ComPtr<ID3D12PipelineState> volumePSO;
	std::shared_ptr<RaymarchedVolume> flame;

	//---------------------Raytracing vars-------------------

	bool raster;
	ComPtr<ID3D12Resource> bottomLevelAs; //storage for bottom level as
	nv_helpers_dx12::TopLevelASGenerator topLevelAsGenerator;
	AccelerationStructureBuffers topLevelAsBuffers;
	std::vector<std::pair<ComPtr<ID3D12Resource>, XMMATRIX>> instances;

	//dxr root signatures
	ComPtr<ID3D12RootSignature> rayGenRootSig;
	ComPtr<ID3D12RootSignature> missRootSig;
	ComPtr<ID3D12RootSignature> closestHitRootSignature;

	//raytracing pipeline state
	ComPtr<ID3D12StateObject> rtStateObject;

	//pipeline state properties, for the shader table
	ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;

	//dxil libs for the shaders
	ComPtr<IDxcBlob> rayGenLib;
	ComPtr<IDxcBlob> missLib;
	ComPtr<IDxcBlob> hitLib;

	ManagedResource rtOutPut;
	//gbuffers resources
	ManagedResource rtPosition;
	ManagedResource rtNormals;
	ManagedResource rtDiffuse;
	DescriptorHeapWrapper rtDescriptorHeap;

	//SBT variables
	nv_helpers_dx12::ShaderBindingTableGenerator sbtGenerator;
	ComPtr<ID3D12Resource> sbtResource;

	//camera
	RayTraceCameraData rtCamera;
	ManagedResource cameraData;
	UINT8* cameraBufferBegin;

	//skybox
	ManagedResource skyboxTexResource;

	//shadow ray variables
	ComPtr<IDxcBlob> shadowRayLib;
	ComPtr<ID3D12RootSignature> shadowRootSig;
	
	//-------------------------------------------------------

	//particle data
	ComPtr<ID3D12PipelineState> particlesPSO;
	ComPtr<ID3D12RootSignature> particleRootSig;

	std::shared_ptr<Emitter> emitter1;

	//ECS variables
	entt::registry registry;

	//flocking variables
	std::vector<std::shared_ptr<Entity>> flockers;

	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;
};

