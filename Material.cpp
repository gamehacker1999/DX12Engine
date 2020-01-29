#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(std::wstring diffuse, ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue,
	DescriptorHeapWrapper mainBufferHeap)
{
	/*LoadTexture(device, diffuseTexture.resource, diffuse, commandQueue,TEXTURE_TYPE_DEAULT);
	CreateShaderResourceView(device.Get(), diffuseTexture.resource.Get(), srvHandle);*/
	mainBufferHeap.CreateDescriptor(diffuse,diffuseTexture,RESOURCE_TYPE_SRV,device,commandQueue,TEXTURE_TYPE_DEAULT);

	//srvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
}
