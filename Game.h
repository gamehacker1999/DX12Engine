#pragma once
//#include "optix_world.h"
#include "DXCore.h"
#include"Renderer.h"
#include <DXRHelper.h>
#include "Camera.h"
#include"Mesh.h"
#include"Entity.h"
#include"Emitter.h"
#include"Lights.h"
#include"DescriptorHeapWrapper.h"
#include"CommonStructs.h"
#include"Material.h"
#include"InteriorMaterial.h"
#include"Skybox.h"
#include"GPUHeapRingBuffer.h"
#include"RaymarchedVolume.h"
#include<Audio.h>
#include<Keyboard.h>
#include<Mouse.h>
#include"RootIndices.h"

#include <array>
#include <io.h>

#include"Velocity.h"

#include <DirectXMath.h>
#define ENTT_STANDARD_CPP
#include<entity\registry.hpp>
#include"Flocker.h"

struct LightData
{
	DirectionalLight light1;
	Vector3 cameraPosition;
	float padding;
};

struct LightingData
{
	Vector3 cameraPosition;
	UINT lightCount;
};

struct LightCullingExternalData
{
	Matrix view;
	Matrix projection;
	Matrix inverseProjection;
	Vector3 cameraPosition;
	int lightCount;
};



struct TAAExternData
{
	Matrix prevView;
	Matrix prevProjection;
	Matrix inverseProjection;
	Matrix inverseView;
};


struct VelocityConstantBuffer
{
	Matrix view;
	Matrix projection;
	Matrix world;
	Matrix prevView;
	Matrix prevProjection;
	Matrix prevWorld;
};

struct BMFRPreProcessData
{
	Matrix view;
	Matrix proj;
	Matrix prevView;
	Matrix prevProj;
	UINT frame_number;
	UINT IMAGE_WIDTH;
	UINT IMAGE_HEIGHT;
};


inline ID3D12Resource* CreateRBBuffer(ID3D12Resource* buffer, ID3D12Device* device, UINT bufferSize)
{
	if (buffer != nullptr)
	{
		buffer->Release();
	}

	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	HRESULT hres = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buffer));
	if (FAILED(hres))
	{
		throw std::runtime_error("Failed to create readback buffer!");
		return nullptr;
	}
	return buffer;
}

struct RayTraceCameraData
{
	Matrix view;
	Matrix proj;
	Matrix iView;
	Matrix iProj;
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
	void InitComputeEngine();
	void InitializeGUI();
	HRESULT InitEnvironment();
	void OnResize();
	void ExecuteAndResetGraphicsCommandList(ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12CommandAllocator> commandAllocator[], ComPtr<ID3D12PipelineState> pipelineState,
		ComPtr<ID3D12CommandQueue> aCommandQueue);

	//------------------Raytracing Functions--------------------------

	//create the acceleration structure for the buffers
	AccelerationStructureBuffers CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>,uint32_t>> vertexBuffers);
	//create top level acceleration structures
	void CreateTopLevelAS(const std::vector<EntityInstance>& instances, bool updateOnly = false);
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
	void CreateRayTracingDirectLightingPipeline();
	void CreateRayTracingIndirectDiffusePipeline();
	void CreateRayTracingIndirectSpecularPipeline();
	void CreateGbufferRaytracingPipeline();

	//shader binding table
	void CreateShaderBindingTable();

	void CreateGBufferRays();
	void CreateDirectRays();
	void CreateIndirectDiffuseRays();
	void CreateIndirectSpecularRays();

	void CreateLTCTexture();
	void PrefilterLTCTextures();

	//----------------------------------------------------------------

	//--------------------------optix functions----------------------

	void InitOptix();
	void SetupDenoising();
	void DenoiseOutput();

	//---------------------------------------------------------------

	//NRD functions ---------------------------------
	void InitNRDDenoiser();


	//-------------------------------------------------

	void RaytracingPrePass();
	void DepthPrePass();
	void BNDSPrePass();
	void RenderVelocityBuffer();
	void LightCullingPass();
	void Update(float deltaTime, float totalTime);
	void UpdateGUI(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void RenderGUI(float deltaTime, float totalTime);
	void RenderEditorWindow();
	void PopulateCommandList();
	void RenderPostProcessing(ManagedResource& inputTexture);
	void WaitForPreviousFrame();
	void MoveToNextFrame();
	void UploadTextureToRingBuffer(std::wstring filename, ManagedResource& resource);

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

	//Compute shader data
	ComPtr<ID3D12CommandAllocator> computeCommandAllocator[frameCount];
	ComPtr<ID3D12CommandQueue> computeCommandQueue;
	ComPtr<ID3D12RootSignature> computeRootSignature;
	ComPtr<ID3D12RootSignature> bndsComputeRootSignature;
	ComPtr<ID3D12GraphicsCommandList> computeCommandList;
	ComPtr<ID3D12PipelineState> computePipelineState;
	ComPtr<ID3D12PipelineState> bndsPipelineState;
	ComPtr<ID3D12Fence> computeFence;
	//

	//normal map filtering
	ComPtr<ID3D12PipelineState> vmfSolverPSO;
	ComPtr<ID3D12RootSignature> vmfSofverRootSignature;

	ComPtr<ID3D12GraphicsCommandList> skyboxBundle;
	ComPtr<ID3D12CommandAllocator> bundleAllocator;

	// The matrices to go from model space to screen space
	Matrix worldMatrix;
	Matrix viewMatrix;
	Matrix projectionMatrix;

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

	ComPtr<ID3D12Resource> lightingConstantBufferResource;
	UINT8* lightingCbufferBegin;
	LightingData lightingData;
	Light* lights;
	UINT8* lightBufferBegin;
	ComPtr<ID3D12Resource> lightListResource;
	UINT lightCount;

	ManagedResource visibleLightIndicesBuffer;
	UINT8* visibleLightIndicesResource;
	UINT* visibleLightIndices;

	//decals
	ComPtr<ID3D12Resource> decalConstanceBufferResource;
	UINT8* decalsCBufferBegin;
	Decal decals[256];
	UINT8* decalSBufferBegin;
	ComPtr<ID3D12Resource> decalListResource;
	UINT decalCount;

	ManagedResource visibleDecalIndicesBuffer;

	LightCullingExternalData lightCullingExternData;
	UINT8* lightCullingExternBegin;
	ComPtr<ID3D12Resource> lightCullingCBVResource;

	//velocity pass
	VelocityConstantBuffer velocityBufferData;
	std::vector<UINT8*> velocityDataBegin;
	ComPtr<ID3D12Resource> velocityCBVData;
	ManagedResource velocityBuffer;

	TAAExternData taaData;
	UINT8* taaDataBegin;
	ComPtr<ID3D12Resource> taaCBResource;

	//blue noise permulation variables
	ManagedResource blueNoiseTex;
	ManagedResource sampleSequences;


	//Light culling variables
	ComPtr<ID3D12Resource> visibleLightList;
	ComPtr<ID3D12Resource> lightGrid;

	//BMFR preprocess data----------------------
	BMFRPreProcessData bmfrPreProcData;
	UINT8* bmfrPreprocessBegin;
	ComPtr<ID3D12Resource> bmfrPreProcessCBV;

	//-----------------------------------------

	//

	//ComPtr<ID3D12Resource> depthStencilBuffer;
	ManagedResource depthStencilBuffer;
	//ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;
	DescriptorHeapWrapper dsDescriptorHeap;

	ManagedResource depthStencilBuffer2;

	//post processing
	DescriptorHeapWrapper renderTargetSRVHeap;
	ManagedResource finalRenderTarget;
	ManagedResource editorWindowTarget;
	ManagedResource guiTarget;
	ManagedResource taaOutput;
	ManagedResource tonemappingOutput;
	ManagedResource sharpenOutput;
	ManagedResource fxaaOutput;
	ManagedResource taaInput;
	ManagedResource taaHistoryBuffer;
	ManagedResource rtCombineOutput;
	UINT numFrames;
	XMFLOAT2* jitters;
	Vector2 prevJitters;
	Vector2 currentJitters;

	std::shared_ptr<Camera> mainCamera;

	std::shared_ptr<Mesh> mesh1;
	std::shared_ptr<Entity> entity1;
	std::shared_ptr<Mesh> mesh2;
	std::shared_ptr<Mesh> mesh3;
	std::shared_ptr<Mesh> skyDome;
	std::shared_ptr<Mesh> sharkMesh;
	std::shared_ptr<Mesh> faceMesh;
	std::shared_ptr<Mesh> rect;
	std::shared_ptr<Mesh> disk;
	std::shared_ptr<Entity> entity2;
	std::shared_ptr<Entity> entity3;
	std::shared_ptr<Entity> entity4;
	std::shared_ptr<Entity> entity5;
	std::shared_ptr<Entity> entity6;
	std::shared_ptr<Entity> diskEntity;
	std::shared_ptr<Entity> cerebrus;
	std::shared_ptr<Material> material1;
	std::shared_ptr<Material> material2;
	std::shared_ptr<InteriorMaterial> interiorMaterial;
	std::shared_ptr<Material> skinMat;

	bool enableSSS;


	std::vector<std::shared_ptr<Entity>> entities;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::string> entityNames;

	//environment variables
	ComPtr<ID3D12RootSignature> skyboxRootSignature;
	ComPtr<ID3D12PipelineState> skyboxPSO;
	std::shared_ptr<Skybox> skybox;

	//ring buffer
	std::shared_ptr<GPUHeapRingBuffer> gpuHeapRingBuffer;

	//pbr pipeline state
	ComPtr<ID3D12PipelineState> pbrPipelineState;

	//subsurface scattering
	ComPtr<ID3D12PipelineState> sssPipelineState;

	//Interior mapping
	ComPtr<ID3D12PipelineState> interiorMappingPSO;
	ComPtr<ID3D12RootSignature> interiorMappingRootSig;

	ManagedResource depthTex;
	ComPtr<ID3D12PipelineState> depthPrePassPipelineState;
	DescriptorHeapWrapper depthDesc;

	//managing the residency
	D3DX12Residency::ResidencyManager residencyManager;
	std::shared_ptr<D3DX12Residency::ResidencySet> residencySet;

	ComPtr<ID3D12RootSignature> volumeRootSignature;
	ComPtr<ID3D12PipelineState> volumePSO;
	std::shared_ptr<RaymarchedVolume> flame;

	//tonemapping variables
	ComPtr<ID3D12RootSignature> toneMappingRootSig;
	ComPtr<ID3D12PipelineState> toneMappingPSO;

	//taa vars
	ComPtr<ID3D12RootSignature> taaRootSig;
	ComPtr<ID3D12PipelineState> taaPSO;

	//sharpen vars
	ComPtr<ID3D12RootSignature> sharpenRootSig;
	ComPtr<ID3D12PipelineState> sharpenPSO;

	//FXAA vars
	ComPtr<ID3D12RootSignature> fxaaRootSig;
	ComPtr<ID3D12PipelineState> fxaaPSO;

	//passthrough vars
	ComPtr<ID3D12RootSignature> passthroughRootSig;
	ComPtr<ID3D12PipelineState> passthroughPSO;

	//rt combine vars
	ComPtr<ID3D12RootSignature> rtCombineRootSig;
	ComPtr<ID3D12PipelineState> rtCombinePSO;


	//veloity vars
	ComPtr<ID3D12RootSignature> velRootSig;
	ComPtr<ID3D12PipelineState> velPSO;

	//---------------------Raytracing vars-------------------

	bool raster;
	bool isRaytracingAllowed;
	bool inlineRaytracing;
	bool rtToggle;
	ComPtr<ID3D12Resource> bottomLevelAs; //storage for bottom level as
	nv_helpers_dx12::TopLevelASGenerator topLevelAsGenerator;
	AccelerationStructureBuffers topLevelAsBuffers;
	std::vector<EntityInstance> bottomLevelBufferInstances;
	ComPtr<ID3D12Resource> previousBuffer;

	//dxr root signatures
	ComPtr<ID3D12RootSignature> rayGenRootSig;
	ComPtr<ID3D12RootSignature> missRootSig;
	ComPtr<ID3D12RootSignature> closestHitRootSignature;

	//raytracing pipeline state
	ComPtr<ID3D12StateObject> rtStateObject;
	ComPtr<ID3D12StateObject> indirectDiffuseRtStateObject;
	ComPtr<ID3D12StateObject> indirectSpecularRtStateObject;
	ComPtr<ID3D12StateObject> gbufferStateObject;

	//pipeline state properties, for the shader table
	ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
	ComPtr<ID3D12StateObjectProperties> indirectDiffuseRtStateObjectProps;
	ComPtr<ID3D12StateObjectProperties> indirectSpecularRtStateObjectProps;
	ComPtr<ID3D12StateObjectProperties> GBrtStateObjectProps;

	//dxil libs for the shaders
	ComPtr<IDxcBlob> rayGenLib;
	ComPtr<IDxcBlob> indirectDiffuseRayGenLib;
	ComPtr<IDxcBlob> indirectSpecularRayGenLib;
	ComPtr<IDxcBlob> missLib;
	ComPtr<IDxcBlob> hitLib;

	ComPtr<IDxcBlob> GBrayGenLib;
	ComPtr<IDxcBlob> GBmissLib;
	ComPtr<IDxcBlob> GBhitLib;

	ManagedResource rtOutPut;
	ManagedResource rtIndirectDiffuseOutPut;
	ManagedResource rtIndirectSpecularOutPut;

	ManagedResource tempRTIndDiffuse;
	ManagedResource tempRTIndSpec;
	//gbuffers resources
	ManagedResource rtPosition;
	ManagedResource rtNormals;
	ManagedResource rtDiffuse;
	ManagedResource rtAlbedo;
	ManagedResource prevNormals;
	ManagedResource prevPosition;
	ManagedResource prevNoisy;
	ManagedResource currentNoisy;
	ManagedResource acceptBools;
	ManagedResource outPrevFramePixels;
	DescriptorHeapWrapper rtDescriptorHeap;

	//SBT variables
	nv_helpers_dx12::ShaderBindingTableGenerator sbtGenerator;
	ComPtr<ID3D12Resource> sbtResource;
	nv_helpers_dx12::ShaderBindingTableGenerator indirectDiffuseSbtGenerator;
	ComPtr<ID3D12Resource> indirectDiffuseSbtResource;
	nv_helpers_dx12::ShaderBindingTableGenerator indirectSpecularSbtGenerator;
	ComPtr<ID3D12Resource> indirectSpecularSbtResource;
	nv_helpers_dx12::ShaderBindingTableGenerator GBsbtGenerator;
	ComPtr<ID3D12Resource> GBsbtResource;

	//camera
	RayTraceCameraData rtCamera;
	ManagedResource cameraData;
	UINT8* cameraBufferBegin;

	//skybox
	ManagedResource skyboxTexResource;

	//shadow ray variables
	ComPtr<IDxcBlob> shadowRayLib;
	ComPtr<ID3D12RootSignature> shadowRootSig;

	//Linearly transformed cosines
	DescriptorHeapWrapper ltcDescriptorHeap;
	DescriptorHeapWrapper ltcTempDescriptorHeap;
	ComPtr<ID3D12Resource> ltcTextureUploadHeap;
	ManagedResource ltcLUT;
	ManagedResource ltcLUT2;
	ManagedResource ltcTexture[8];
	ManagedResource ltcTexture0;
	ManagedResource ltcTexture1;
	ManagedResource ltcTexture2;
	ManagedResource ltcTexture3;
	ManagedResource ltcTexture4;
	ManagedResource ltcTexture5;
	ManagedResource ltcTexture6;
	ManagedResource ltcTexture7;

	ManagedResource ltcPrefilterTexture;

	DescriptorHeapWrapper prefilterRTVHeap;
	
	//-------------------------------------------------------

	//----------------------Otix-----------------------------
	//optix::Context optixContext;
	//optix::Buffer inputBuffer;
	//ComPtr<ID3D12Resource> readbackInputBuffer;
	//UINT inputSlot;
	//optix::Buffer normalBuffer;
	//ComPtr<ID3D12Resource> readbackNormalBuffer;
	//ComPtr<ID3D12Resource> outputNormalBuffer;
	//D3D12_GPU_DESCRIPTOR_HANDLE normalBufferTextureUavDescriptorHandleGPU;
	//UINT normalSlot;
	//optix::Buffer albedoBuffer;
	//ComPtr<ID3D12Resource> readbackAlbedoBuffer;
	//ComPtr<ID3D12Resource> outputAlbedoBuffer;
	//D3D12_GPU_DESCRIPTOR_HANDLE albedoBufferTextureUavDescriptorHandleGPU;
	//UINT albedoSlot;
	//
	//optix::Buffer outBuffer;
	//optix::PostprocessingStage denoiserStage;
	//optix::CommandList optiXCommandList;
	//unsigned char* denoised_pixels = new unsigned char[width * height * 4];
	//ComPtr<ID3D12Resource> textureUploadBuffer;
	//bool mDenoiseOutput = true;

	//-------------------------------------------------------

	//image based lighting 

		//pipeline state objects
	ComPtr<ID3D12PipelineState> irradiencePSO;
	ComPtr<ID3D12PipelineState> prefilteredMapPSO;
	ComPtr<ID3D12PipelineState> brdfLUTPSO;

	//root signatures
	ComPtr<ID3D12RootSignature> irradianceRootSignature;
	ComPtr<ID3D12RootSignature> prefilteredRootSignature;
	ComPtr<ID3D12RootSignature> brdfRootSignature;

	//------------------------------------

	//particle data
	ComPtr<ID3D12PipelineState> particlesPSO;
	ComPtr<ID3D12RootSignature> particleRootSig;

	std::shared_ptr<Emitter> emitter1;
	std::vector<std::shared_ptr<Emitter>> emitters;

	//ECS variables
	entt::registry registry;

	//flocking variables
	std::vector<std::shared_ptr<Entity>> flockers;

	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;

	ImGuizmo::OPERATION gizmoMode;
	bool entityManipulated;
	bool addNewEntity;
	int pickingIndex;
};

