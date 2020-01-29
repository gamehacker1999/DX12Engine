#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper& mainBufferHeap,
	std::wstring diffuse, std::wstring normal, std::wstring roughness, std::wstring metallnes)
{

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

	//srvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
}

UINT Material::GetMaterialOffset()
{
	return materialOffset+1;
}

UINT Material::GetDiffuseTextureOffset()
{
	return diffuseTextureIndex;
}
