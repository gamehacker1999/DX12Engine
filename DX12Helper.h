#pragma once
#include<d3dcompiler.h>
#include<random>
#include"ResourceUploadBatch.h"
#include <WICTextureLoader.h>
#include <Keyboard.h>
#include<Mouse.h>
#include<DDSTextureLoader.h>
#include<d3d12.h>
#include"d3dx12Residency.h"
#include<DirectXMath.h>
#include"Vertex.h"
#include<wrl\client.h>
#include<stdexcept>
#include"d3dx12.h"
#include"RootIndices.h"
#include <SimpleMath.h>

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/ImGuizmo.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

using namespace DirectX;
using namespace DirectX::SimpleMath;

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
	ComPtr<ID3D12Fence> graphicsFence;
	ComPtr<ID3D12Fence> computeFence;
	UINT64* fenceValues;
	UINT frameIndex;
	HANDLE fenceEvent;
	D3D12_HEAP_PROPERTIES defaultHeapType;
	D3D12_HEAP_PROPERTIES uploadHeapType;
	D3D12_RANGE zeroZeroRange;
};

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

inline float halton(int i, int b)
{
	/* Creates a halton sequence of values between 0 and 1.
	https://en.wikipedia.org/wiki/Halton_sequence
	Used for jittering based on a constant set of 2D points. */
	float f = 1.0;
	float r = 0.0;
	while (i > 0)
	{
		f = f / float(b);
		r = r + f * float(i % b);
		i = i / b;
	}
	return r;
}

inline Vector2* GenerateHaltonJitters()
{
	Vector2* jitters = new Vector2[16];

	for (size_t i = 0; i < 16; i++)
	{
		jitters[i] = Vector2((2.0f*halton(i+1, 2)-1.0f), (2.0f*halton(i+1, 3) - 1.0f));
	}

	return jitters;
}

struct EulerAngles {
	double roll, pitch, yaw;
};

inline EulerAngles ToEulerAngles(Quaternion q)
{
	EulerAngles angles;

	// roll (x-axis rotation)
	double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	angles.roll = std::atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	double sinp = 2 * (q.w * q.y - q.z * q.x);
	if (std::abs(sinp) >= 1)
		angles.pitch = std::copysign(XM_PI / 2, sinp); // use 90 degrees if out of range
	else
		angles.pitch = std::asin(sinp);

	// yaw (z-axis rotation)
	double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	angles.yaw = std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

inline float Float3Distance(Vector3 pos1, Vector3 pos2)
{
	XMVECTOR vector1 = XMLoadFloat3(&pos1);
	XMVECTOR vector2 = XMLoadFloat3(&pos2);
	XMVECTOR vectorSub = XMVectorSubtract(vector1, vector2);
	XMVECTOR length = XMVector3Length(vectorSub);

	float distance = 0.0f;
	XMStoreFloat(&distance, length);
	return distance;
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

//These should match the defines in Common.hlsl
enum RaytracingInstanceMask
{
	RAYTRACING_INSTANCE_OPAQUE = 1 << 0,
	RAYTRACING_INSTANCE_TRANSCLUCENT = 1 << 1,
	RAYTRACING_INSTANCE_ALL = 0xFF
};

struct EntityInstance
{
	UINT entityIndex;
	ComPtr<ID3D12Resource> bottomLevelBuffer;
	DirectX::XMMATRIX modelMatrix;
	RaytracingInstanceMask instanceMask;
};

inline Vector3 GetRandomFloat3(float minRange, float maxRange)
{
	//random position and veloctiy of the particle
	std::random_device rd;
	std::mt19937 randomGenerator(rd());
	std::uniform_real_distribution<float> dist(minRange,maxRange);

	return Vector3(dist(randomGenerator), dist(randomGenerator), dist(randomGenerator));
}

void InitResources(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12GraphicsCommandList> computeCommandList,
ComPtr<ID3D12CommandQueue> graphicsQueue, ComPtr<ID3D12CommandAllocator> commandAllocators[3],
ComPtr<ID3D12CommandQueue> computeQueue, ComPtr<ID3D12CommandAllocator> computeAllocator[3],
ComPtr<ID3D12Fence> graphicsFence, ComPtr<ID3D12Fence> computeFence, UINT64 fenceValues[3], HANDLE fenceEvent);

void WaitToFlushGPU(ComPtr<ID3D12CommandQueue> commandQueue,ComPtr<ID3D12Fence> fence, UINT64 fenceValue,HANDLE fenceEvent);

using namespace Microsoft::WRL;

D3D12_VERTEX_BUFFER_VIEW CreateVBView(Vertex* vertexData, unsigned int numVerts, ComPtr<ID3D12Resource>& vertexBufferHeap, ComPtr<ID3D12Resource>& uploadHeap);

D3D12_INDEX_BUFFER_VIEW CreateIBView(unsigned int* indexData, unsigned int numIndices, ComPtr<ID3D12Resource>& indexBufferHeap, ComPtr<ID3D12Resource>& uploadIndexHeap);

void LoadTexture(ComPtr<ID3D12Resource>& tex, std::wstring textureName, 
	ID3D12Resource* uploadRes = nullptr,
	TEXTURE_TYPES type=TEXTURE_TYPE_DEAULT);

void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);

void GenerateMipMaps(ComPtr<ID3D12Resource>& texture);

void PrefilterLTCTexture(ComPtr<ID3D12Resource>& texture);

void DenoiseBMFR(ManagedResource inputTex, ManagedResource inputNorm, ManagedResource inputWorld,
	ManagedResource inputAlbedo, ManagedResource prevNorm, ManagedResource prevWorld,
	ManagedResource prevAlbedo);

void BMFRPreprocess(ManagedResource rtOutput, ManagedResource normals, ManagedResource position,
	ManagedResource prevOutput, ManagedResource prevNormals, ManagedResource prevPos,
	ManagedResource acceptBools, ManagedResource outPrevPixelFrame, ComPtr<ID3D12Resource> cbvData,
	UINT frameIndex, ComPtr<ID3D12DescriptorHeap> srvUavHeap, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

void BMFRRegression(ManagedResource rtOutput, ManagedResource normals, ManagedResource position,
	ManagedResource prevOutput, ManagedResource prevNormals, ManagedResource prevPos,
	ManagedResource acceptBools, ManagedResource outPrevPixelFrame, ComPtr<ID3D12Resource> cbvData,
	UINT frameIndex, ComPtr<ID3D12DescriptorHeap> srvUavHeap);

ComPtr<ID3D12PipelineState> CreatePipelineState();

void TransitionManagedResource(const ComPtr<ID3D12GraphicsCommandList>& commandList, ManagedResource& resource, D3D12_RESOURCE_STATES afterState);

void CopyResource(ComPtr<ID3D12GraphicsCommandList6>& commandList, ManagedResource& dst, ManagedResource& src);
// Computes a compute shader dispatch size given a thread group size, and number of elements to process
UINT DispatchSize(UINT tgSize, UINT numElements);

float* ReadHDR(const wchar_t* textureFile, unsigned int* width, unsigned int* height);

void LoadPrefilteredLTCTexture(ManagedResource tex);

ApplicationResources& GetAppResources();

void SubmitGraphicsCommandList(const ComPtr<ID3D12GraphicsCommandList>& commandList);

void SubmitComputeCommandList(const ComPtr<ID3D12GraphicsCommandList>& computeCommandList, const ComPtr<ID3D12GraphicsCommandList>& commandList);




