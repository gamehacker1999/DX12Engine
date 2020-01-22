#include"DX12Helper.h"
#include"Game.h"

void WaitToFlushGPU(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, UINT64 fenceValue, HANDLE fenceEvent)
{
	//signal and increment the fence
	const UINT64 pfence = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), pfence));
	fenceValue++;

	//wait until the previous frame is finished
	if (fence->GetCompletedValue() < pfence)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(pfence, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

D3D12_VERTEX_BUFFER_VIEW CreateVBView(Vertex* vertexData, unsigned int numVerts, ComPtr<ID3D12Device> device,
	ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& vertexBufferHeap, ComPtr<ID3D12Resource>& uploadHeap
	)
{
	{
		UINT vertexBufferSize = numVerts*sizeof(Vertex);

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(vertexBufferHeap.GetAddressOf())
		));

		vertexBufferHeap->SetName(L"vertex default heap");

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap.GetAddressOf())
		));
		uploadHeap->SetName(L"Upload heap");
		D3D12_SUBRESOURCE_DATA bufferData = {};
		bufferData.pData = reinterpret_cast<BYTE*>(vertexData);
		bufferData.RowPitch = vertexBufferSize;
		bufferData.SlicePitch = vertexBufferSize;

		UpdateSubresources<1>(commandList.Get(), vertexBufferHeap.Get(), uploadHeap.Get(), 0, 0, 1, &bufferData);
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

		//WaitToFlushGPU

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		vertexBufferView.BufferLocation = vertexBufferHeap->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vertexBufferSize;

		return vertexBufferView;
	}
}

D3D12_INDEX_BUFFER_VIEW CreateIBView(unsigned int* indexData, unsigned int numIndices, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12Resource>& indexBufferHeap, ComPtr<ID3D12Resource>& uploadIndexHeap)
{
	{
		UINT indexBufferSize = numIndices * sizeof(unsigned int);

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(indexBufferHeap.GetAddressOf())
		));

		indexBufferHeap->SetName(L"index default heap");

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadIndexHeap.GetAddressOf())
		));
		uploadIndexHeap->SetName(L"Upload index heap");
		D3D12_SUBRESOURCE_DATA bufferData = {};
		bufferData.pData = reinterpret_cast<BYTE*>(indexData);
		bufferData.RowPitch = indexBufferSize;
		bufferData.SlicePitch = indexBufferSize;

		UpdateSubresources<1>(commandList.Get(), indexBufferHeap.Get(), uploadIndexHeap.Get(), 0, 0, 1, &bufferData);
		//copy triangle data to vertex buffer
		UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
		//ThrowIfFailed(vbufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
		//memcpy(vertexDataBegin, triangleVBO, sizeof(triangleVBO));
		//vbufferUpload->Unmap(0, nullptr);

		/*commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_COPY_DEST));
		commandList->CopyResource(vertexBuffer.Get(), vbufferUpload.Get());*/
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDEX_BUFFER));

		//command lists are created in record state but since there is nothing to record yet
		//close it for the main loop
		//WaitToFlushGPU

		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		indexBufferView.BufferLocation = indexBufferHeap->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = indexBufferSize;

		return indexBufferView;
	}
}
