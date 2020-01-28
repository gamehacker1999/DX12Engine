#pragma once
#include"ResourceUploadBatch.h"
#include <WICTextureLoader.h>
#include<DDSTextureLoader.h>
#include<d3d12.h>
#include<DirectXMath.h>
#include"Vertex.h"
#include<wrl\client.h>
#include<stdexcept>
#include"d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

class Game;

typedef enum TEXTURE_TYPES
{
	TEXTURE_TYPE_DDS,
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

struct ManagedResource
{
	ComPtr<ID3D12Resource> resource;
	UINT64 heapOffset;
	D3D12_RESOURCE_STATES currentState;
};


void WaitToFlushGPU(ComPtr<ID3D12CommandQueue> commandQueue,ComPtr<ID3D12Fence> fence, UINT64 fenceValue,HANDLE fenceEvent);

using namespace Microsoft::WRL;

D3D12_VERTEX_BUFFER_VIEW CreateVBView(Vertex* vertexData, unsigned int numVerts,ComPtr<ID3D12Device> device,
	ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& vertexBufferHeap, ComPtr<ID3D12Resource>& uploadHeap);

D3D12_INDEX_BUFFER_VIEW CreateIBView(unsigned int* indexData, unsigned int numIndices, ComPtr<ID3D12Device> device,
	ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& indexBufferHeap, ComPtr<ID3D12Resource>& uploadIndexHeap);

void LoadTexture(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& tex, std::wstring textureName, ComPtr<ID3D12CommandQueue>& commandQueue,TEXTURE_TYPES type=TEXTURE_TYPE_DEAULT);
