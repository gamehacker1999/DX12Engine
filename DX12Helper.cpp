#include "DX12Helper.h"
#include <fstream>


float* pixels;

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
		//UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu
		//ThrowIfFailed(vbufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
		//memcpy(vertexDataBegin, triangleVBO, sizeof(triangleVBO));
		//vbufferUpload->Unmap(0, nullptr);

		/*commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_COPY_DEST));
		commandList->CopyResource(vertexBuffer.Get(), vbufferUpload.Get());*/
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER| D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

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
		//UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDEX_BUFFER));

		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		indexBufferView.BufferLocation = indexBufferHeap->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = indexBufferSize;

		return indexBufferView;
	}
}

void LoadTexture(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& tex, std::wstring textureName, ComPtr<ID3D12CommandQueue>& commandQueue, 
    ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12Resource* uploadRes, TEXTURE_TYPES type)
{

	if (type == TEXTURE_TYPE_DDS)
	{
		ResourceUploadBatch resourceUpload(device.Get());
		resourceUpload.Begin();

		ThrowIfFailed(CreateDDSTextureFromFile(device.Get(), resourceUpload, textureName.c_str(), tex.GetAddressOf(), true));

		auto uploadResourceFinish = resourceUpload.End(commandQueue.Get());

		uploadResourceFinish.wait();
	}

    else if (type == TEXTURE_TYPE_HDR)
    {
        unsigned int width = 0;
        unsigned int height = 0;
        pixels = ReadHDR(textureName.c_str(), &width, &height);

        D3D12_RESOURCE_DESC textDesc = {};
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(tex.GetAddressOf())
        ));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadRes)));

        // Copy data to the intermediate upload heap and the
        // from the upload heap to the Texture2D

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = pixels;
        textureData.RowPitch = width * 16;
        textureData.SlicePitch = textureData.RowPitch * height;

        UpdateSubresources<1>(commandList.Get(), tex.Get(), uploadRes, 0, 0, 1, &textureData);

        delete[] pixels;

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    }

	else
	{
		//loading texture from filename
		ResourceUploadBatch resourceUpload(device.Get());

		resourceUpload.Begin();

		ThrowIfFailed(CreateWICTextureFromFile(device.Get(), resourceUpload, textureName.c_str(), tex.GetAddressOf(), true));

		auto uploadedResourceFinish = resourceUpload.End(commandQueue.Get());

		uploadedResourceFinish.wait();
	}


}

_Use_decl_annotations_
void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}

UINT DispatchSize(UINT tgSize, UINT numElements)
{
	UINT dispatchSize = numElements / tgSize;
	dispatchSize += numElements % tgSize > 0 ? 1 : 0;
	return dispatchSize;
}


// Reads an HDR file
// Based on: http://www.cs.virginia.edu/~jcw5q/apps/imageview/src/libimageviewer/hdr.c
float* ReadHDR(const wchar_t* textureFile, unsigned int* width, unsigned int* height)
{
    const char* HeaderSignature = "#?RADIANCE";
    const char* HeaderFormat = "FORMAT=32-bit_rle_rgbe";
    const unsigned int HeaderSigSize = 10;
    const unsigned int HeaderFormatSize = 22;
    // Basic read buffer
    char buffer[1024] = { 0 };
    bool invX = false;
    bool invY = false;
    // HEADER -----------------------------------
    // Open the file
    std::fstream file(textureFile, std::ios_base::in | std::ios_base::binary);
    // Read the signature
    file.read(buffer, HeaderSigSize);
    if (strcmp(HeaderSignature, buffer) != 0)
        return false;
    // Skip comment until we find FORMAT
    do {
        file.getline(buffer, sizeof(buffer));
    } while (!file.eof() && strncmp(buffer, "FORMAT", 6));
    // Did we hit the end of the file already?
    if (file.eof()) return false;
    // Invalid format!
    if (strcmp(buffer, HeaderFormat) != 0)
        return false;
    // Check for Y inversion
    int x = 0;
    do {
        x++;
        file.getline(buffer, sizeof(buffer));
    } while (!file.eof() && (
        strncmp(buffer, "-Y", 2) &&
        strncmp(buffer, "+Y", 2))
        );
    // End of file while looking?
    if (file.eof()) return false;
    // Inverted?
    if (strncmp(buffer, "-Y", 2) == 0) invY = true;
    // Loop through buffer until X
    int counter = 0;
    while ((counter < sizeof(buffer)) && buffer[counter] != 'X')
        counter++;
    // No X?
    if (counter == sizeof(buffer)) return false;
    // Flipped X?
    if (buffer[counter - 1] == '-') invX = true;
    // Grab dimensions from current buffer line
    sscanf_s(buffer, "%*s %u %*s %u", height, width);
    // Got real dimensions?
    if ((*width) == 0 || (*height) == 0)
        return false;
    // ACTUAL DATA ------------------------------
    unsigned int dataSize = (*width) * (*height) * 4;
    unsigned char* data = new unsigned char[dataSize];
    memset(data, 0, dataSize); // For testing
    // Scanline encoding
    char enc[4] = { 0 };
    // Loop through the scanlines, one at a time
    for (unsigned int y = 0; y < (*height); y++)
    {
        // Inverted Y doesn't seem to matter?
        int start = /*invY ? ((*height) - y - 1) * (*width) :*/ y * (*width);
        int step = /*invX ? -1 :*/ 1;
        // Check the encoding info for this line
        file.read(enc, 4);
        if (file.eof())
            break;
        // Which RLE scheme?
        if (enc[0] == 2 && enc[1] == 2 && enc[2] >= 0)
        {
            // NEW RLE SCHEME -------------------
            // Each component is RLE'd separately
            for (int c = 0; c < 4; c++)
            {
                int pos = start;
                unsigned int x = 0;
                // Loop through the pixels/components
                while (x < (*width))
                {
                    char temp;
                    file.read(&temp, 1);
                    unsigned char num = reinterpret_cast<unsigned char&>(temp);
                    // Check if this is a run
                    if (num <= 128)
                    {
                        // No run, read the data
                        for (int i = 0; i < num; i++)
                        {
                            char temp;
                            file.read(&temp, 1);
                            data[c + pos * 4] = reinterpret_cast<unsigned char&>(temp);
                            pos += step;
                        }
                    }
                    else
                    {
                        // Run!  Get the value and set everything
                        char temp;
                        file.read(&temp, 1);
                        unsigned char value = reinterpret_cast<unsigned char&>(temp);
                        num -= 128;
                        for (int i = 0; i < num; i++)
                        {
                            data[c + pos * 4] = value;
                            pos += step;
                        }
                    }
                    // Move to the next section
                    x += num;
                }
            }
        }
        else
        {
            // OLD RLE SCHEME -------------------
            int pos = start;
            // Loop through scanline
            for (unsigned int x = 0; x < (*width); x++)
            {
                if (x > 0)
                {
                    file.read(enc, 4); // TODO: check for eof?
                }
                // Check for RLE header
                if (enc[0] == 1 && enc[1] == 1 && enc[2] == 1)
                {
                    // RLE
                    int num = ((int)enc[3]) & 0xFF;
                    unsigned char r = data[(pos - step) * 4 + 0];
                    unsigned char g = data[(pos - step) * 4 + 1];
                    unsigned char b = data[(pos - step) * 4 + 2];
                    unsigned char e = data[(pos - step) * 4 + 3];
                    // Loop and set
                    for (int i = 0; i < num; i++)
                    {
                        data[pos * 4 + 0] = r;
                        data[pos * 4 + 1] = g;
                        data[pos * 4 + 2] = b;
                        data[pos * 4 + 3] = e;
                        pos += step;
                    }
                    x += num - 1;
                }
                else
                {
                    // No RLE, just read data
                    data[pos * 4 + 0] = enc[0];
                    data[pos * 4 + 1] = enc[1];
                    data[pos * 4 + 2] = enc[2];
                    data[pos * 4 + 3] = enc[3];
                    pos += step;
                }
            }
        }
    }
    // Done with file
    file.close();
    // Convert data to final IEEE floats
    // Based on "Real Pixels" by Greg Ward in Graphics Gems II
    float* pixels = new float[(*width) * (*height) * 4];
    for (unsigned int i = 0; i < (*width) * (*height); i++)
    {
        unsigned char exponent = data[i * 4 + 3];
        if (exponent == 0)
        {
            pixels[i * 4 + 0] = 0.0f;
            pixels[i * 4 + 1] = 0.0f;
            pixels[i * 4 + 2] = 0.0f;
            pixels[i * 4 + 3] = 1.0f;
        }
        else
        {
            float v = ldexp(1.0f / 256.0f, (int)(exponent - 128));
            pixels[i * 4 + 0] = (data[i * 4 + 0] + 0.5f) * v;
            pixels[i * 4 + 1] = (data[i * 4 + 1] + 0.5f) * v;
            pixels[i * 4 + 2] = (data[i * 4 + 2] + 0.5f) * v;
            pixels[i * 4 + 3] = 1.0f;
        }
    }

    auto lol = pixels[2400];

    // Clean up
    delete[] data;
    // Success
    return pixels;
}