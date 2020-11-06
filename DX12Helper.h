#pragma once
#include<d3dcompiler.h>
#include"DescriptorHeapWrapper.h"
#include"GPUHeapRingBuffer.h"
#include<random>
#include"ResourceUploadBatch.h"
#include <WICTextureLoader.h>
#include<DDSTextureLoader.h>
#include<d3d12.h>
#include"d3dx12Residency.h"
#include<DirectXMath.h>
#include"Vertex.h"
#include<wrl\client.h>
#include<stdexcept>
#include"d3dx12.h"
#include"RootIndices.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

using namespace DirectX;

class Game;

struct ApplicationResources
{
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> graphicsQueue;
	ComPtr<ID3D12CommandAllocator>* commandAllocators;
	ComPtr<ID3D12CommandQueue> computeQueue;
	ComPtr<ID3D12CommandAllocator>* computeAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12GraphicsCommandList> computeCommandList;
	std::shared_ptr<GPUHeapRingBuffer> gpuHeapRingBuffer;
	UINT frameIndex;

	//Utitlity Resources
	ComPtr<ID3D12PipelineState> generateMipMapsPSO;
	ComPtr<ID3D12RootSignature> generateMipMapsRootSig;
	DescriptorHeapWrapper srvUavCBVDescriptorHeap;
};

ApplicationResources appResources;

typedef enum TEXTURE_TYPES
{
	TEXTURE_TYPE_DDS,
	TEXTURE_TYPE_HDR,
	TEXTURE_TYPE_DEAULT	
}TEXTURE_TYPES;

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

typedef enum RESOURCE_TYPE
{
	RESOURCE_TYPE_CBV,
	RESOURCE_TYPE_SRV,
	RESOURCE_TYPE_RTV,
	RESOURCE_TYPE_DSV,
	RESOURCE_TYPE_UAV
} RESOURCE_TYPE;

struct ManagedResource
{
	ComPtr<ID3D12Resource> resource;
	UINT heapOffset;
	D3D12_RESOURCE_STATES currentState;
	RESOURCE_TYPE resourceType;

	D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE uavGPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE cbvGPUHandle;

	D3D12_GPU_DESCRIPTOR_HANDLE rtvGPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE dsvGPUHandle;

	D3D12_GPU_DESCRIPTOR_HANDLE ringBufferGPUHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE uavCPUHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE cbvCPUHandle;


	D3D12_CPU_DESCRIPTOR_HANDLE rtvCPUHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvCPUHandle;

	float width;
	float height;
	float mipLevels;
};


struct AccelerationStructureBuffers
{
	ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
	ComPtr<ID3D12Resource> pResult;       // Where the AS is
	ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
	UINT srvIndex;
};

struct EntityInstance
{
	UINT entityIndex;
	ComPtr<ID3D12Resource> bottomLevelBuffer;
	DirectX::XMMATRIX modelMatrix;
};

inline XMFLOAT3 GetRandomFloat3(float minRange, float maxRange)
{
	//random position and veloctiy of the particle
	std::random_device rd;
	std::mt19937 randomGenerator(rd());
	std::uniform_real_distribution<float> dist(minRange,maxRange);

	return XMFLOAT3(dist(randomGenerator), dist(randomGenerator), dist(randomGenerator));
}

void InitResources(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12GraphicsCommandList> computeCommandList,
ComPtr<ID3D12CommandQueue> graphicsQueue, ComPtr<ID3D12CommandAllocator> commandAllocators[3],
ComPtr<ID3D12CommandQueue> computeQueue, ComPtr<ID3D12CommandAllocator> computeAllocator[3],
std::shared_ptr<GPUHeapRingBuffer> gpuHeapRingBuffer);

void WaitToFlushGPU(ComPtr<ID3D12CommandQueue> commandQueue,ComPtr<ID3D12Fence> fence, UINT64 fenceValue,HANDLE fenceEvent);

using namespace Microsoft::WRL;

D3D12_VERTEX_BUFFER_VIEW CreateVBView(Vertex* vertexData, unsigned int numVerts,ComPtr<ID3D12Device> device,
	ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& vertexBufferHeap, ComPtr<ID3D12Resource>& uploadHeap);

D3D12_INDEX_BUFFER_VIEW CreateIBView(unsigned int* indexData, unsigned int numIndices, ComPtr<ID3D12Device> device,
	ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& indexBufferHeap, ComPtr<ID3D12Resource>& uploadIndexHeap);

void LoadTexture(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& tex, std::wstring textureName, 
	ComPtr<ID3D12CommandQueue>& commandQueue,
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr, ID3D12Resource* uploadRes = nullptr,
	TEXTURE_TYPES type=TEXTURE_TYPE_DEAULT);

void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);

void GenerateMipMaps(ComPtr<ID3D12Resource> texture);

ComPtr<ID3D12PipelineState> CreatePipelineState();

// Computes a compute shader dispatch size given a thread group size, and number of elements to process
UINT DispatchSize(UINT tgSize, UINT numElements);

float* ReadHDR(const wchar_t* textureFile, unsigned int* width, unsigned int* height);



