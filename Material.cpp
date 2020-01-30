#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper& mainBufferHeap,
	ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig,
	std::wstring diffuse, std::wstring normal, std::wstring roughness,
	std::wstring metallnes)
{

	this->rootSignature = rootSig;
	this->pipelineState = pipelineState;

	//materialIndex = 0;
	numTextures = 0;
	/*LoadTexture(device, diffuseTexture.resource, diffuse, commandQueue,TEXTURE_TYPE_DEAULT);
	CreateShaderResourceView(device.Get(), diffuseTexture.resource.Get(), srvHandle);*/
	mainBufferHeap.CreateDescriptor(diffuse,diffuseTexture,RESOURCE_TYPE_SRV,device,commandQueue,TEXTURE_TYPE_DEAULT);
	diffuseTextureIndex = diffuseTexture.heapOffset;
	materialOffset = diffuseTexture.heapOffset;
	//materialIndex = index;
	numTextures++;

	if (normal != L"default")
	{
		mainBufferHeap.CreateDescriptor(normal, normalTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = normalTexture.heapOffset;
		numTextures++;
	}

	if (roughness != L"default")
	{
		mainBufferHeap.CreateDescriptor(roughness, roughnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	if (metallnes != L"default")
	{
		mainBufferHeap.CreateDescriptor(metallnes, metallnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
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

UINT Material::GetMaterialOffset()
{
	return materialOffset+1;
}

UINT Material::GetDiffuseTextureOffset()
{
	return diffuseTextureIndex;
}
