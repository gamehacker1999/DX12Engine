#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(std::wstring diffuse, ComPtr<ID3D12Device>& device, ComPtr<ID3D12CommandQueue>& commandQueue, DescriptorHeapWrapper mainBufferHeap)
{
	LoadTexture(device, diffuseTexture, diffuse, commandQueue,TEXTURE_TYPE_DEAULT);
	CreateShaderResourceView(device.Get(), diffuseTexture.Get(), srvHandle);
	srvHandle.Offset(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
}
