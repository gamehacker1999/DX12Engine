#include "Ocean.h"

Ocean::Ocean(std::shared_ptr<Mesh> waterMesh,
	ComPtr<ID3D12RootSignature> graphicsRootSignature, ComPtr<ID3D12PipelineState> graphicsPipelineState,
	ComPtr<ID3D12PipelineState> computePipelineState, ComPtr<ID3D12RootSignature> computeRootSignature,
	ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> graphicsCommandList, ComPtr<ID3D12CommandList> computeCommandList)
{
	this->waterMesh = waterMesh;
	this->graphicsRootSignature = graphicsRootSignature;
	this->computeRootSignature = computeRootSignature;
	this->graphicsPipelineState = graphicsPipelineState;
	this->computePipelineState = computePipelineState;

	//creating the necessary descriptor heaps
	rtvDescriptorHeap.Create(device, 6, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	srvcbvuavDescriptorHeap.Create(device, 24, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//creating the necessary textures, SRVs, and UAVs
	texSize = 256;

	//creating the h0 texture
	D3D12_RESOURCE_DESC h0TexDesc = {};
	h0TexDesc.Width = texSize;
	h0TexDesc.Height = texSize;
	h0TexDesc.DepthOrArraySize = 1;
	h0TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	h0TexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	h0TexDesc.MipLevels = 1;
	h0TexDesc.SampleDesc.Count = 1;
	h0TexDesc.SampleDesc.Quality = 0;
	h0TexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(h0Texture.resource.GetAddressOf())));

	h0Texture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating h0 uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(h0Texture,RESOURCE_TYPE_SRV,device,0,texSize,texSize,0,1);
	srvcbvuavDescriptorHeap.CreateDescriptor(h0Texture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the h0minus texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(h0MinusTexture.resource.GetAddressOf())));

	h0MinusTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating h0 minus uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(h0MinusTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(h0MinusTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the hty texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(htyTexture.resource.GetAddressOf())));

	htyTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating hty uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(htyTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(htyTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the htx texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(htxTexture.resource.GetAddressOf())));
	htxTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating hty uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(htxTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(htxTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the hty texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(htzTexture.resource.GetAddressOf())));
	htzTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating hty uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(htzTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(htzTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the twiddle indices texture
	int bits = (int)(log(texSize), log(2));
	h0TexDesc.Width = bits;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(twiddleTexture.resource.GetAddressOf())));
	twiddleTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating twiddle uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(twiddleTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(twiddleTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	h0TexDesc.Width = texSize;

	//creating the pingpong 0 texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(pingpong0Texture.resource.GetAddressOf())));

	pingpong0Texture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating pingpong uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(pingpong0Texture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(pingpong0Texture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the dy texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(dyTexture.resource.GetAddressOf())));
	dyTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating dy uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(dyTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(dyTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the dx texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(dxTexture.resource.GetAddressOf())));
	dxTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating dx uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(dxTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(dxTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the dz texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(dzTexture.resource.GetAddressOf())));
	dzTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating dz uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(dzTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(dzTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the normal texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(normalMapTexture.resource.GetAddressOf())));
	normalMapTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating normal map uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(normalMapTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(normalMapTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

	//creating the folding map texture
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &h0TexDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(foldingMapTexture.resource.GetAddressOf())));
	foldingMapTexture.currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	//creating folding map uav and srv
	srvcbvuavDescriptorHeap.CreateDescriptor(foldingMapTexture, RESOURCE_TYPE_SRV, device, 0, texSize, texSize, 0, 1);
	srvcbvuavDescriptorHeap.CreateDescriptor(foldingMapTexture, RESOURCE_TYPE_UAV, device, 0, texSize, texSize, 0, 1);

}

Ocean::~Ocean()
{
}

void Ocean::Update(float deltaTime)
{
}

void Ocean::Draw(std::shared_ptr<Camera> camera, ComPtr<ID3D12GraphicsCommandList> commandList, float deltaTime, float totalTime)
{
}

void Ocean::CreateH0Texture()
{
}

void Ocean::CreateHtTexture(float totalTime)
{
}

int Ocean::CreateBitReversedIndices(int num, int d)
{
	unsigned int count = sizeof(num) * 8 - 1;
	unsigned int reverseNum = num;

	num >>= 1;
	while (num)
	{
		reverseNum <<= 1;
		reverseNum |= num & 1;
		num >>= 1;
		count--;
	}
	//this is our bit reversed number
	reverseNum <<= count;

	//rotating this number left
	return (reverseNum << d) | (reverseNum >> (CHAR_BIT * sizeof(reverseNum) - d));
}

void Ocean::CreateTwiddleIndices()
{
}

void Ocean::RenderFFT(float totalTime)
{
}
