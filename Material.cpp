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
