#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper& mainBufferHeap,
	ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig,
	std::wstring diffuse, std::wstring normal, std::wstring roughness,
	std::wstring metallnes)
{

	ThrowIfFailed(descriptorHeap.Create(device, 4, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	this->rootSignature = rootSig;
	this->pipelineState = pipelineState;

	//materialIndex = 0;
	numTextures = 0;
	/*LoadTexture(device, diffuseTexture.resource, diffuse, commandQueue,TEXTURE_TYPE_DEAULT);
	CreateShaderResourceView(device.Get(), diffuseTexture.resource.Get(), srvHandle);*/
	descriptorHeap.CreateDescriptor(diffuse,diffuseTexture,RESOURCE_TYPE_SRV,device,commandQueue,TEXTURE_TYPE_DEAULT);
	diffuseTextureIndex = diffuseTexture.heapOffset;
	materialOffset = diffuseTexture.heapOffset;
	//materialIndex = index;
	numTextures++;

	if (normal != L"default")
	{
		descriptorHeap.CreateDescriptor(normal, normalTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = normalTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultNormal.png", normalTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = normalTexture.heapOffset;
		numTextures++;
	}

	if (roughness != L"default")
	{
		descriptorHeap.CreateDescriptor(roughness, roughnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultRoughness.png", roughnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	if (metallnes != L"default")
	{
		descriptorHeap.CreateDescriptor(metallnes, metallnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultMetallic.png", metallnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	//srvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	dummyHeap.Create(device, 2, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CopyDescriptorsSimple(2, dummyHeap.GetCPUHandle(0), descriptorHeap.GetCPUHandle(1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//set pso and root signature TODO

	//setting the necessary variables
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorHeap.GetHeap().Get() };

	
}

void Material::GenerateMaps(ComPtr<ID3D12Device> device)
{
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1, roughnessTexture.mipLevels,1,0,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(generatedRoughnessMap.resource.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1, roughnessTexture.mipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(vmfMap.resource.GetAddressOf())
	));


}

ComPtr<ID3D12RootSignature>& Material::GetRootSignature()
{
	return rootSignature;
}

ComPtr<ID3D12PipelineState>& Material::GetPipelineState()
{
	return pipelineState;
}

DescriptorHeapWrapper& Material::GetDescriptorHeap()
{
	return descriptorHeap;
}

UINT Material::GetMaterialOffset()
{
	return materialOffset+1;
}

UINT Material::GetDiffuseTextureOffset()
{
	return diffuseTextureIndex;
}
