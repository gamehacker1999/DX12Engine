#pragma once
#include <WICTextureLoader.h>
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

using namespace Microsoft::WRL;

D3D12_VERTEX_BUFFER_VIEW CreateVertexBuffer(Vertex* vertexData, ComPtr<ID3D12Device> device, 
	ComPtr<ID3D12GraphicsCommandList> commandList,ComPtr<ID3D12Resource> vertexBufferHeap,ComPtr<ID3D12CommandQueue> commandQueue)
{
	UINT vertexBufferSize = sizeof(vertexData);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(vertexBufferHeap.GetAddressOf())
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
	bufferData.pData = reinterpret_cast<BYTE*>(vertexData);
	bufferData.RowPitch = vertexBufferSize;
	bufferData.SlicePitch = vertexBufferSize;

	UpdateSubresources<1>(commandList.Get(), vertexBufferHeap.Get(), vbufferUpload.Get(), 0, 0, 1, &bufferData);
	//copy triangle data to vertex buffer
	UINT8* vertexDataBegin;
	CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
	//ThrowIfFailed(vbufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
	//memcpy(vertexDataBegin, triangleVBO, sizeof(triangleVBO));
	//vbufferUpload->Unmap(0, nullptr);

	/*commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_COPY_DEST));
	commandList->CopyResource(vertexBuffer.Get(), vbufferUpload.Get());*/
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));



	//command lists are created in record state but since there is nothing to record yet
	//close it for the main loop

	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	//WaitForPreviousFrame();

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = vertexBufferHeap->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = sizeof(vertexData);

	return vertexBufferView;
}