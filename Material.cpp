#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(DescriptorHeapWrapper& mainBufferHeap, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig,
	std::wstring diffuse, std::wstring normal, std::wstring roughness, std::wstring metalness)
{

	ThrowIfFailed(descriptorHeap.Create(GetAppResources().device, 4, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	this->rootSignature = rootSig;
	this->pipelineState = pipelineState;

	//materialIndex = 0;
	numTextures = 0;
	descriptorHeap.CreateDescriptor(diffuse,diffuseTexture,RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue,TEXTURE_TYPE_DEAULT);
	diffuseTextureIndex = diffuseTexture.heapOffset;
	materialOffset = diffuseTexture.heapOffset;
	//materialIndex = index;
	numTextures++;

	if (normal != L"default")
	{
		descriptorHeap.CreateDescriptor(normal, normalTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = normalTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultNormal.png", normalTexture, RESOURCE_TYPE_SRV, GetAppResources().device,
			GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = normalTexture.heapOffset;
		numTextures++;
	}

	if (roughness != L"default")
	{
		descriptorHeap.CreateDescriptor(roughness, roughnessTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultRoughness.png", roughnessTexture, RESOURCE_TYPE_SRV, GetAppResources().device,
			GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	if (metalness != L"default")
	{
		descriptorHeap.CreateDescriptor(metalness, metallnessTexture, RESOURCE_TYPE_SRV, GetAppResources().device, GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultMetallic.png", metallnessTexture, RESOURCE_TYPE_SRV, GetAppResources().device,
			GetAppResources().graphicsQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1,
		roughnessTexture.mipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(generatedRoughnessMap.resource.GetAddressOf())
	));

	texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1,
		roughnessTexture.mipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
		&GetAppResources().defaultHeapType,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(vmfMap.resource.GetAddressOf())
	));

	for (int i = 0; i < roughnessTexture.mipLevels; i++)
	{

		ComPtr<ID3D12Resource> resource;
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
		//creating the cbuffer
		ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
			&GetAppResources().uploadHeapType,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource.GetAddressOf())
		));

		generateMapDataResource.emplace_back(resource);

		GenerateMapExternData mapData = {};

		mapData.outputSize = XMFLOAT2(roughnessTexture.width, roughnessTexture.height);
		mapData.textureSize = XMFLOAT2(roughnessTexture.width, roughnessTexture.height);

		generateMapData.emplace_back(mapData);

		//ZeroMemory(&generateMapData[i], sizeof(GenerateMapExternData));

		UINT8* bufferBegin = 0;
		generateMapDataResource[i]->Map(0, &GetAppResources().zeroZeroRange, reinterpret_cast<void**>(&bufferBegin));

		generateMapDataCbufferBegin.emplace_back(bufferBegin);

	}

	//setting the necessary variables
	generateMapDescriptorHeap.Create(GetAppResources().device, roughnessTexture.mipLevels * 2 + 2 + 2, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//auto transition = CD3DX12_RESOURCE_BARRIER::Transition(normalTexture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	//	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	//GetAppResources().commandList->ResourceBarrier(1, &transition);
	//transition = CD3DX12_RESOURCE_BARRIER::Transition(roughnessTexture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	//	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	//GetAppResources().commandList->ResourceBarrier(1, &transition);

}

void Material::GenerateMaps(ComPtr<ID3D12PipelineState> vmfSolverPSO, ComPtr<ID3D12RootSignature> vmfRootSig,
	std::shared_ptr<GPUHeapRingBuffer> gpuRingBuffer)
{


	GetAppResources().device->CopyDescriptorsSimple(2, generateMapDescriptorHeap.GetCPUHandle(0), descriptorHeap.GetCPUHandle(1),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	generateMapDescriptorHeap.IncrementLastResourceIndex(2);

	auto texWidth = roughnessTexture.width;
	auto texHeight = roughnessTexture.height;

	ID3D12DescriptorHeap* ppHeaps[] = { generateMapDescriptorHeap.GetHeapPtr() };
	GetAppResources().computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	GetAppResources().computeCommandList->SetComputeRootSignature(vmfRootSig.Get());
	GetAppResources().computeCommandList->SetPipelineState(vmfSolverPSO.Get());

	for (int i = 0; i < roughnessTexture.mipLevels; i++)
	{
		generateMapData[i].mipLevel = i;
		generateMapData[i].outputSize = XMFLOAT2(texWidth, texHeight);

		generateMapDescriptorHeap.CreateDescriptor(vmfMap, RESOURCE_TYPE_UAV, GetAppResources().device, 0,
			roughnessTexture.width, roughnessTexture.height,
			0, i);
		generateMapDescriptorHeap.CreateDescriptor(generatedRoughnessMap, RESOURCE_TYPE_UAV, GetAppResources().device, 0,
			roughnessTexture.width, roughnessTexture.height,
			0, i);
		memcpy(generateMapDataCbufferBegin[i], &generateMapData[i], sizeof(generateMapData[i]));

		GetAppResources().computeCommandList->SetComputeRootDescriptorTable(VMFFilterRootIndices::NormalRoughnessSRV, generateMapDescriptorHeap.GetGPUHandle(0));
		GetAppResources().computeCommandList->SetComputeRootDescriptorTable(VMFFilterRootIndices::OutputRoughnessVMFUAV, vmfMap.uavGPUHandle);
		GetAppResources().computeCommandList->SetComputeRootConstantBufferView(VMFFilterRootIndices::VMFFilterExternDataCBV, generateMapDataResource[i]->GetGPUVirtualAddress());
		GetAppResources().computeCommandList->SetComputeRoot32BitConstant(VMFFilterRootIndices::VMFFilterNumParameters, i, 0);


		GetAppResources().computeCommandList->Dispatch(DispatchSize(16, texWidth), DispatchSize(16, texHeight), 1);

		texWidth = std::max<UINT>(texWidth / 2, 1);
		texHeight = std::max<UINT>(texHeight / 2, 1);
	}


	gpuRingBuffer->GetDescriptorHeap().CreateDescriptor(vmfMap, RESOURCE_TYPE_SRV, GetAppResources().device, 0, roughnessTexture.width,
		roughnessTexture.height, 0, roughnessTexture.mipLevels);
	gpuRingBuffer->GetDescriptorHeap().CreateDescriptor(generatedRoughnessMap, RESOURCE_TYPE_SRV, GetAppResources().device, 0, roughnessTexture.width,
		roughnessTexture.height, 0, roughnessTexture.mipLevels);

	gpuRingBuffer->IncrementNumStaticResources(2);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(generatedRoughnessMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);

	transition = CD3DX12_RESOURCE_BARRIER::Transition(vmfMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);

	transition = CD3DX12_RESOURCE_BARRIER::Transition(normalTexture.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);

	transition = CD3DX12_RESOURCE_BARRIER::Transition(roughnessTexture.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GetAppResources().commandList->ResourceBarrier(1, &transition);

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
