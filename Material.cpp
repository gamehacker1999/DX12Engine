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

	materialIndex = diffuseTexture.heapOffset;
	numTextures++;

	if (normal != L"default")
	{
		mainBufferHeap.CreateDescriptor(normal, normalTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		numTextures++;
	}

	//srvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
}

UINT Material::GetMatIndex()
{
	return materialIndex;
}

UINT Material::GetNumMaterials()
{
	return numTextures;
}

UINT Material::GetMaterialOffset()
{
	return numTextures * (materialIndex+1);
}
