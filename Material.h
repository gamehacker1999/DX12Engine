#pragma once
#include"DX12Helper.h"
#include<string>
#include"DescriptorHeapWrapper.h"
#include"GPUHeapRingBuffer.h"

struct GenerateMapExternData
{
	Vector2 outputSize;
	Vector2 textureSize;
	UINT mipLevel;
};

using namespace DirectX;
class Material
{

protected:
	//ComPtr<ID3D12Resource> diffuseTexture;
	ManagedResource diffuseTexture;
	ManagedResource normalTexture;
	ManagedResource roughnessTexture;
	ManagedResource metallnessTexture;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	DescriptorHeapWrapper descriptorHeap;
	DescriptorHeapWrapper dummyHeap; 
	DescriptorHeapWrapper generateMapDescriptorHeap; //descriptorHeap for vmf solver
	UINT materialOffset;
	UINT diffuseTextureIndex;
	UINT numTextures;
	D3D12_SHADER_RESOURCE_VIEW_DESC diffuseSRV;

	//Generated roughness and vmf maps
	ManagedResource generatedRoughnessMap;
	ManagedResource vmfMap;

	//cbuffer
	std::vector<GenerateMapExternData> generateMapData;
	std::vector<ComPtr<ID3D12Resource>> generateMapDataResource;
	std::vector<UINT8*> generateMapDataCbufferBegin;

public:
	Material(int numTextures = 5);
	Material(std::wstring diffuse = L"default", std::wstring normal = L"default", std::wstring roughness = L"default",
		std::wstring metallnes = L"default");

	void GenerateMaps(ComPtr<ID3D12PipelineState> vmfSolverPSO, ComPtr<ID3D12RootSignature> vmfRootSig,
		std::shared_ptr<GPUHeapRingBuffer> gpuRingBuffer);

	ComPtr<ID3D12RootSignature>& GetRootSignature();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	virtual DescriptorHeapWrapper& GetDescriptorHeap();
	UINT GetMaterialOffset();
	UINT GetDiffuseTextureOffset();

	bool filteredNormalMap;


	UINT materialIndex;
	UINT prefilteredMapIndex;
};

