#include "InteriorMaterial.h"

InteriorMaterial::InteriorMaterial(int numTex): Material(numTex)
{
	ThrowIfFailed(descriptorHeap.Create(GetAppResources().device, numTex + 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	ThrowIfFailed(textureArrayHeap.Create(GetAppResources().device, 5, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	interiorMaps = new ManagedResource[5];

	auto commandList = GetAppResources().commandList;

	descriptorHeap.CreateDescriptor(L"../../Assets/Textures/Interiors/OfficeCubeMap.dds", interiorMaps[0], RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DDS);
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[0].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	descriptorHeap.CreateDescriptor(L"../../Assets/Textures/Interiors/OfficeCubeMapBrick.dds", interiorMaps[1], RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DDS);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[1].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	descriptorHeap.CreateDescriptor(L"../../Assets/Textures/Interiors/OfficeCubeMapBrown.dds", interiorMaps[2], RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DDS);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[2].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	descriptorHeap.CreateDescriptor(L"../../Assets/Textures/Interiors/OfficeCubeMapBrownDark.dds", interiorMaps[3], RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DDS);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[3].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	descriptorHeap.CreateDescriptor(L"../../Assets/Textures/Interiors/OfficeCubeMapDark.dds", interiorMaps[4], RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DDS);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[4].resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transition);

	numTextures += numTex;

	auto desc = interiorMaps[0].resource->GetDesc();
	//creating the texture cube array
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Format = desc.Format;
	resourceDesc.MipLevels = desc.MipLevels;
	resourceDesc.DepthOrArraySize = 6 * numTex;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Width = 256;
	resourceDesc.Height = 256;


	ThrowIfFailed
	(
		GetAppResources().device->CreateCommittedResource
		(
			&GetAppResources().defaultHeapType,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(textureArray.resource.GetAddressOf())
		)
	);

	textureArray.resource->SetName(L"Texture Cube Array");

	D3D12_BOX box = {};
	box.top = 0;
	box.left = 0;
	box.bottom = 256;
	box.right = 256;
	box.front = 0;
	box.back = 1;

	// Copy all textures into this one
	for (int cube = 0; cube < numTex; cube++)
	{
		for (int face = 0; face < 6; face++)
		{
			for (int mip = 0; mip < desc.MipLevels; mip++)
			{
				// Update the box for this mip
				box.right = 256;
				box.bottom = box.right;

				// Copy
				D3D12_TEXTURE_COPY_LOCATION destLoc = {};
				destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				destLoc.pResource = textureArray.resource.Get();
				destLoc.SubresourceIndex = D3D12CalcSubresource(mip, cube * 6 + face, 0, desc.MipLevels, 0);

				D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				srcLoc.pResource = interiorMaps[cube].resource.Get();
				srcLoc.SubresourceIndex = D3D12CalcSubresource(mip, face, 0, desc.MipLevels, 0);

				commandList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, &box);
				
			}
		}
	}

	transition = CD3DX12_RESOURCE_BARRIER::Transition(textureArray.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[0].resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[1].resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[2].resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[3].resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);
	transition = CD3DX12_RESOURCE_BARRIER::Transition(interiorMaps[4].resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);

	textureArrayHeap.CreateDescriptor(textureArray, RESOURCE_TYPE_SRV, GetAppResources().device, 0, 0, 0, 0, desc.MipLevels);
	textureArrayHeap.CreateDescriptor(L"../../Assets/Textures/ExternalWall.png", externalTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
	textureArrayHeap.CreateDescriptor(L"../../Assets/Textures/BrickWall.png", capTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
	textureArrayHeap.CreateDescriptor(L"../../Assets/Textures/ExternalWallSDF.png", sdfTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);

}

InteriorMaterial::~InteriorMaterial()
{
	if (interiorMaps)
		delete[] interiorMaps;
}

DescriptorHeapWrapper& InteriorMaterial::GetDescriptorHeap()
{
	return textureArrayHeap;
}

ManagedResource& InteriorMaterial::GetExternalTexture()
{
	return externalTexture;
}
