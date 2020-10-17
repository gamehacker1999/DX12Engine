#include "Material.h"
#include<DirectXHelpers.h>

Material::Material(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& commandQueue, 
	DescriptorHeapWrapper& mainBufferHeap,
	ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	std::wstring diffuse, std::wstring normal, std::wstring roughness,
	std::wstring metalness)
{

	ThrowIfFailed(descriptorHeap.Create(device, 4, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	this->rootSignature = rootSig;
	this->pipelineState = pipelineState;

	//materialIndex = 0;
	numTextures = 0;
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
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultNormal.png", normalTexture, RESOURCE_TYPE_SRV, device, 
			commandQueue, TEXTURE_TYPE_DEAULT);
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
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultRoughness.png", roughnessTexture, RESOURCE_TYPE_SRV, device, 
			commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = roughnessTexture.heapOffset;
		numTextures++;
	}

	if (metalness != L"default")
	{
		descriptorHeap.CreateDescriptor(metalness, metallnessTexture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	else
	{
		descriptorHeap.CreateDescriptor(L"../../Assets/Textures/DefaultMetallic.png", metallnessTexture, RESOURCE_TYPE_SRV, device, 
			commandQueue, TEXTURE_TYPE_DEAULT);
		materialOffset = metallnessTexture.heapOffset;
		numTextures++;
	}

	//setting the necessary variables
	generateMapDescriptorHeap.Create(device, roughnessTexture.mipLevels * 2 + 2 + 2, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalTexture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(roughnessTexture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

}

void Material::GenerateMaps(ComPtr<ID3D12Device> device,
	ComPtr<ID3D12PipelineState> vmfSolverPSO, ComPtr<ID3D12RootSignature> vmfRootSig,
	ComPtr<ID3D12GraphicsCommandList> computeCommandList,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	std::shared_ptr<GPUHeapRingBuffer> gpuRingBuffer)
{
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1, 
			roughnessTexture.mipLevels,1,0,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(generatedRoughnessMap.resource.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, roughnessTexture.width, roughnessTexture.height, 1, 
			roughnessTexture.mipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(vmfMap.resource.GetAddressOf())
	));

	for (int i = 0; i < roughnessTexture.mipLevels; i++)
	{

		ComPtr<ID3D12Resource> resource;
		//creating the cbuffer
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
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
		generateMapDataResource[i]->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&bufferBegin));

		generateMapDataCbufferBegin.emplace_back(bufferBegin);

	}

	device->CopyDescriptorsSimple(2, generateMapDescriptorHeap.GetCPUHandle(0), descriptorHeap.GetCPUHandle(1), 
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	generateMapDescriptorHeap.IncrementLastResourceIndex(2);

	auto texWidth = roughnessTexture.width;
	auto texHeight = roughnessTexture.height;

	ID3D12DescriptorHeap* ppHeaps[] = { generateMapDescriptorHeap.GetHeapPtr() };
	computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	computeCommandList->SetComputeRootSignature(vmfRootSig.Get());
	computeCommandList->SetPipelineState(vmfSolverPSO.Get());

	for (int i = 0; i < roughnessTexture.mipLevels; i++)
	{
		generateMapData[i].mipLevel = i;
		generateMapData[i].outputSize = XMFLOAT2(texWidth, texHeight);

		generateMapDescriptorHeap.CreateDescriptor(vmfMap, RESOURCE_TYPE_UAV, device, 0,
			roughnessTexture.width, roughnessTexture.height,
			0, i);
		generateMapDescriptorHeap.CreateDescriptor(generatedRoughnessMap, RESOURCE_TYPE_UAV, device, 0, 
			roughnessTexture.width, roughnessTexture.height,
			0, i);
		memcpy(generateMapDataCbufferBegin[i], &generateMapData[i], sizeof(generateMapData[i]));

		computeCommandList->SetComputeRootDescriptorTable(VMFFilterRootIndices::NormalRoughnessSRV, generateMapDescriptorHeap.GetGPUHandle(0));
		computeCommandList->SetComputeRootDescriptorTable(VMFFilterRootIndices::OutputRoughnessVMFUAV, vmfMap.uavGPUHandle);
		computeCommandList->SetComputeRootConstantBufferView(VMFFilterRootIndices::VMFFilterExternDataCBV, generateMapDataResource[i]->GetGPUVirtualAddress());
		computeCommandList->SetComputeRoot32BitConstant(VMFFilterRootIndices::VMFFilterNumParameters, i, 0);


		computeCommandList->Dispatch(DispatchSize(16, texWidth), DispatchSize(16, texHeight), 1);

		texWidth = std::max<UINT>(texWidth / 2, 1);
		texHeight = std::max<UINT>(texHeight / 2, 1);
	}

	gpuRingBuffer->GetDescriptorHeap().CreateDescriptor(vmfMap, RESOURCE_TYPE_SRV, device, 0, roughnessTexture.width,
		roughnessTexture.height, 0, roughnessTexture.mipLevels);
	gpuRingBuffer->GetDescriptorHeap().CreateDescriptor(generatedRoughnessMap, RESOURCE_TYPE_SRV, device, 0, roughnessTexture.width,
		roughnessTexture.height, 0, roughnessTexture.mipLevels);

	gpuRingBuffer->IncrementNumStaticResources(2);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(generatedRoughnessMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vmfMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalTexture.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(roughnessTexture.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

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
