#include "DX12Helper.h"
#include <fstream>


ApplicationResources appResources;

//Utitlity Resources
ComPtr<ID3D12PipelineState> generateMipMapsPSO;
ComPtr<ID3D12RootSignature> generateMipMapsRootSig;
ComPtr<ID3D12DescriptorHeap> srvUavCBVDescriptorHeap;
UINT lastResourceIndex;
CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
UINT handleIncrementSize;

//Utitlity Resources
ComPtr<ID3D12PipelineState> ltcTexPrefilterPSO;
ComPtr<ID3D12RootSignature> ltcTexPrefilterRootSig;

//BMFR Preprocess Resources
ComPtr<ID3D12PipelineState> bmfrPreProcessPSO;
ComPtr<ID3D12RootSignature> bmfrPreProcessRootSig;

//BMFR Regression Resources
ComPtr<ID3D12PipelineState> bmfrRegressionPSO;
ComPtr<ID3D12RootSignature> bmfrRegressionRootSig;

//BMFR Postprocess Resources
ComPtr<ID3D12PipelineState> bmfrPostProcessPSO;
ComPtr<ID3D12RootSignature> bmfrPostProcessRootSig;

auto defaultHeapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
auto uploadHeapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);


void InitResources(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12GraphicsCommandList> computeCommandList,
    ComPtr<ID3D12CommandQueue> graphicsQueue, ComPtr<ID3D12CommandAllocator> commandAllocators[3],
    ComPtr<ID3D12CommandQueue> computeQueue, ComPtr<ID3D12CommandAllocator> computeAllocator[3],
    ComPtr<ID3D12Fence> graphicsFence, ComPtr<ID3D12Fence> computeFence, UINT64 fenceValues[3], HANDLE fenceEvent)
{
    appResources = {};
    appResources.device = device;
    appResources.commandList = commandList;
    appResources.computeCommandList = computeCommandList;
    appResources.computeAllocator = computeAllocator;
    appResources.computeQueue = computeQueue;
    appResources.graphicsQueue = graphicsQueue;
    appResources.commandAllocators = commandAllocators;
    appResources.frameIndex = 0;
    appResources.graphicsFence = graphicsFence;
    appResources.computeFence = computeFence;
    appResources.fenceValues = fenceValues;
    lastResourceIndex = 0;
    appResources.fenceEvent = fenceEvent;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1200;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    appResources.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(srvUavCBVDescriptorHeap.GetAddressOf()));
    appResources.uploadHeapType = uploadHeapType;
    appResources.defaultHeapType = defaultHeapType;
    appResources.zeroZeroRange = CD3DX12_RANGE(0, 0);

   cpuHandle =srvUavCBVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
   gpuHandle =srvUavCBVDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
   handleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    //generating mip maps
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
    
        CD3DX12_ROOT_PARAMETER1 rootParams[3];
        rootParams[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParams[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParams[2].InitAsConstants(2, 0, 0);

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
        computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc);
    
        ComPtr<ID3DBlob> computeSignature;
        ComPtr<ID3DBlob> computeError;
    
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
        ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&generateMipMapsRootSig)));
    
        ComPtr<ID3DBlob> shaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"GenerateMipMapsCS.cso", shaderBlob.GetAddressOf()));
    
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
        computePSODesc.pRootSignature = generateMipMapsRootSig.Get();
        computePSODesc.CS = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());
    
        ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(generateMipMapsPSO.GetAddressOf())));
    }

    //prefilter ltc tex
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

        CD3DX12_ROOT_PARAMETER1 rootParams[3];
        rootParams[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParams[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParams[2].InitAsConstants(5, 0, 0);

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
        computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc);

        ComPtr<ID3DBlob> computeSignature;
        ComPtr<ID3DBlob> computeError;

        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
        ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&ltcTexPrefilterRootSig)));

        ComPtr<ID3DBlob> shaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"PrefilterLTCTextureCS.cso", shaderBlob.GetAddressOf()));

        D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
        computePSODesc.pRootSignature = ltcTexPrefilterRootSig.Get();
        computePSODesc.CS = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

        ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(ltcTexPrefilterPSO.GetAddressOf())));
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    //BMFR preprocess
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[8];
        CD3DX12_ROOT_PARAMETER1 rootParams[BMFRPreProcessRootIndices::BMFRPreProcessNumParams];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

        rootParams[BMFRPreProcessRootIndices::CurPosSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::PrevPosSRV].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::CurNormSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::PrevNormSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::PrevNoisySRV].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::CurNoisyUAV].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::AcceptBoolUAV].InitAsDescriptorTable(1, &ranges[6], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::PrevFramePixelUAV].InitAsDescriptorTable(1, &ranges[7], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPreProcessRootIndices::FrameDataCBC].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];
        staticSamplers[0].Init(0);


        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
            _countof(staticSamplers), staticSamplers, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
            signature.GetAddressOf(), error.GetAddressOf()));

        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(bmfrPreProcessRootSig.GetAddressOf())));

        ComPtr<ID3DBlob> fullcreenVS;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(L"BMFRPreprocessPS.cso", pixelShader.GetAddressOf()));
        ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

        //creating a tonemapping pipeline state
        D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
        PSODesc.InputLayout = { };
        PSODesc.pRootSignature = bmfrPreProcessRootSig.Get();
        PSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
        PSODesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        PSODesc.DepthStencilState.DepthEnable = false;
        PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
        PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
        PSODesc.SampleMask = UINT_MAX;
        PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PSODesc.NumRenderTargets = 1;
        PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        PSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        PSODesc.SampleDesc.Count = 1;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(bmfrPreProcessPSO.GetAddressOf())));

    }

    //BMFR Regression
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[6];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

        CD3DX12_ROOT_PARAMETER1 rootParams[BMFRRegressionRootIndices::BMFRRegressionNumParams];
        rootParams[BMFRRegressionRootIndices::CurPositionSRV].InitAsDescriptorTable(1, &ranges[0]);
        rootParams[BMFRRegressionRootIndices::CurNormalSRV].InitAsDescriptorTable(1, &ranges[1]);
        rootParams[BMFRRegressionRootIndices::CurAlbedoSRV].InitAsDescriptorTable(1, &ranges[2]);
        rootParams[BMFRRegressionRootIndices::CurrentNoisyUAV].InitAsDescriptorTable(1, &ranges[3]);
        rootParams[BMFRRegressionRootIndices::TempDataUAV].InitAsDescriptorTable(1, &ranges[4]);
        rootParams[BMFRRegressionRootIndices::OutDataUAV].InitAsDescriptorTable(1, &ranges[5]);
        rootParams[BMFRRegressionRootIndices::FrameDataCBV].InitAsConstants(4, 0, 0);

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
        computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc);

        ComPtr<ID3DBlob> computeSignature;
        ComPtr<ID3DBlob> computeError;

        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
        ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&bmfrRegressionRootSig)));

        ComPtr<ID3DBlob> shaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"BMFRRegressionCS.cso", shaderBlob.GetAddressOf()));

        D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
        computePSODesc.pRootSignature = bmfrRegressionRootSig.Get();
        computePSODesc.CS = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

        ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(bmfrRegressionPSO.GetAddressOf())));
    }

    //BMFR post process
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[6];
        CD3DX12_ROOT_PARAMETER1 rootParams[BMFRPostProcessNumParams];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        rootParams[BMFRPostProcessRootIndices::FilterFrameSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::AccumPrevFrameSRV].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::AlbedoSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::AcceptBoolsSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::PrevFramePixelSRV].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::AccumFrameUAV].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[BMFRPostProcessRootIndices::FrameDataConstants].InitAsConstants(1, 0);

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];
        staticSamplers[0].Init(0);


        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams,
            _countof(staticSamplers), staticSamplers, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
            signature.GetAddressOf(), error.GetAddressOf()));

        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(bmfrPostProcessRootSig.GetAddressOf())));

        ComPtr<ID3DBlob> fullcreenVS;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(L"BMFRPostprocessPS.cso", pixelShader.GetAddressOf()));
        ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));

        //creating a tonemapping pipeline state
        D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
        PSODesc.InputLayout = { };
        PSODesc.pRootSignature = bmfrPostProcessRootSig.Get();
        PSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
        PSODesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        PSODesc.DepthStencilState.DepthEnable = false;
        PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
        PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
        PSODesc.SampleMask = UINT_MAX;
        PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PSODesc.NumRenderTargets = 1;
        PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        PSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        PSODesc.SampleDesc.Count = 1;
        ThrowIfFailed(device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(bmfrPostProcessPSO.GetAddressOf())));
    }
}

void WaitToFlushGPU(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, UINT64 fenceValue, HANDLE fenceEvent)
{
	//signal and increment the fence
	const UINT64 pfence = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), pfence));
	fenceValue++;

	//wait until the previous frame is finished
	if (fence->GetCompletedValue() < pfence)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(pfence, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

D3D12_VERTEX_BUFFER_VIEW CreateVBView(Vertex* vertexData, unsigned int numVerts, ComPtr<ID3D12Resource>& vertexBufferHeap, ComPtr<ID3D12Resource>& uploadHeap)
{
	{

		UINT vertexBufferSize = numVerts*sizeof(Vertex);

        auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

		ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
			&defaultHeapType,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(vertexBufferHeap.GetAddressOf())
		));

		vertexBufferHeap->SetName(L"vertex default heap");

		ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
			&uploadHeapType,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap.GetAddressOf())
		));
		uploadHeap->SetName(L"Upload heap");
		D3D12_SUBRESOURCE_DATA bufferData = {};
		bufferData.pData = reinterpret_cast<BYTE*>(vertexData);
		bufferData.RowPitch = vertexBufferSize;
		bufferData.SlicePitch = vertexBufferSize;

		UpdateSubresources<1>(GetAppResources().commandList.Get(), vertexBufferHeap.Get(), uploadHeap.Get(), 0, 0, 1, &bufferData);
		//copy triangle data to vertex buffer
		//UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu

        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(vertexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        GetAppResources().commandList->ResourceBarrier(1, &transition);

		//command lists are created in record state but since there is nothing to record yet
		//close it for the main loop

		//WaitToFlushGPU

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		vertexBufferView.BufferLocation = vertexBufferHeap->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vertexBufferSize;

		return vertexBufferView;
	}
}

D3D12_INDEX_BUFFER_VIEW CreateIBView(unsigned int* indexData, unsigned int numIndices, 
	ComPtr<ID3D12Resource>& indexBufferHeap, ComPtr<ID3D12Resource>& uploadIndexHeap)
{
	{
		UINT indexBufferSize = numIndices * sizeof(unsigned int);
        auto indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
			&defaultHeapType,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(indexBufferHeap.GetAddressOf())
		));

		indexBufferHeap->SetName(L"index default heap");

		ThrowIfFailed(GetAppResources().device->CreateCommittedResource(
			&uploadHeapType,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadIndexHeap.GetAddressOf())
		));
		uploadIndexHeap->SetName(L"Upload index heap");
		D3D12_SUBRESOURCE_DATA bufferData = {};
		bufferData.pData = reinterpret_cast<BYTE*>(indexData);
		bufferData.RowPitch = indexBufferSize;
		bufferData.SlicePitch = indexBufferSize;

		UpdateSubresources<1>(GetAppResources().commandList.Get(), indexBufferHeap.Get(), uploadIndexHeap.Get(), 0, 0, 1, &bufferData);
		//copy triangle data to vertex buffer
		//UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); //we do not intend to read from this resource in the cpu

        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(indexBufferHeap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        GetAppResources().commandList->ResourceBarrier(1, &transition);

		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		indexBufferView.BufferLocation = indexBufferHeap->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = indexBufferSize;

		return indexBufferView;
	}
}

void LoadTexture(ComPtr<ID3D12Resource>& tex, std::wstring textureName, ID3D12Resource* uploadRes, TEXTURE_TYPES type)
{

	if (type == TEXTURE_TYPE_DDS)
	{
		ResourceUploadBatch resourceUpload(appResources.device.Get());
		resourceUpload.Begin();

		ThrowIfFailed(CreateDDSTextureFromFile(appResources.device.Get(), resourceUpload, textureName.c_str(), tex.GetAddressOf(), true));

		auto uploadResourceFinish = resourceUpload.End(appResources.graphicsQueue.Get());

		uploadResourceFinish.wait();
	}

    else if (type == TEXTURE_TYPE_HDR)
    {
        unsigned int width = 0;
        unsigned int height = 0;
        float* pixels;
        pixels = ReadHDR(textureName.c_str(), &width, &height);

        auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 5, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(appResources.device->CreateCommittedResource(
            &defaultHeapType,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(tex.GetAddressOf())
        ));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        ThrowIfFailed(appResources.device->CreateCommittedResource(
            &uploadHeapType,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadRes)));

        // Copy data to the intermediate upload heap and the
        // from the upload heap to the Texture2D

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = pixels;
        textureData.RowPitch = width * 16;
        textureData.SlicePitch = textureData.RowPitch * height;

        UpdateSubresources<1>(appResources.commandList.Get(), tex.Get(), uploadRes, 0, 0, 1, &textureData);

        delete[] pixels;

        GenerateMipMaps(tex);
    }

	else
	{   
		//loading texture from filename
		ResourceUploadBatch resourceUpload(appResources.device.Get());

		resourceUpload.Begin();

		ThrowIfFailed(CreateWICTextureFromFileEx(appResources.device.Get(), resourceUpload, textureName.c_str(), 0Ui64, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, WIC_LOADER_MIP_AUTOGEN, tex.GetAddressOf()));

		auto uploadedResourceFinish = resourceUpload.End(appResources.graphicsQueue.Get());

		uploadedResourceFinish.wait();
	}


}

_Use_decl_annotations_
void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}

void GenerateMipMaps(ComPtr<ID3D12Resource>& texture)
{

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    appResources.commandList->ResourceBarrier(1, &transition);

    texture->SetName(L"mip");

    SubmitGraphicsCommandList(appResources.commandList);

    auto desc = texture->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = desc.Format;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    auto width = desc.Width;
    auto height = desc.Height;
    
    ID3D12DescriptorHeap* ppHeaps[] = { srvUavCBVDescriptorHeap.Get() };

    appResources.computeCommandList->SetPipelineState(generateMipMapsPSO.Get());
    appResources.computeCommandList->SetComputeRootSignature(generateMipMapsRootSig.Get());
    appResources.computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    for (int i = 0; i < desc.MipLevels - 1; i++)

    {   

        auto destWidth = std::max<UINT>(width >> (i + 1), 1);
        auto destHeight = std::max<UINT>(height >> (i + 1), 1);

        srvDesc.Texture2D.MostDetailedMip = i;
        uavDesc.Texture2D.MipSlice = i + 1;
        transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i);
        appResources.computeCommandList->ResourceBarrier(1, &transition);

        appResources.device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuHandle);
        cpuHandle.Offset(1, handleIncrementSize);
        lastResourceIndex++;

        appResources.device->CreateUnorderedAccessView(texture.Get(), 0, &uavDesc, cpuHandle);
        cpuHandle.Offset(1, handleIncrementSize);
        lastResourceIndex++;

        appResources.computeCommandList->SetComputeRootDescriptorTable(0, gpuHandle);
        gpuHandle.Offset(1, handleIncrementSize);

        appResources.computeCommandList->SetComputeRootDescriptorTable(1, gpuHandle);
        gpuHandle.Offset(1, handleIncrementSize);

        XMFLOAT2 pixelSize = XMFLOAT2(1.0 / (float)destWidth, 1.0 / (float)destHeight);
        appResources.computeCommandList->SetComputeRoot32BitConstants(2, 2, &pixelSize, 0) ;
        //appResources.computeCommandList->SetComputeRoot32BitConstant(2, 1.0 / destHeight, 1);

        //Dispatch the compute shader with one thread per 8x8 pixels
        appResources.computeCommandList->Dispatch(std::max<UINT>(destWidth / 8, 1u), std::max<UINT>(destHeight / 8, 1u), 1);

        //Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(texture.Get());
        appResources.computeCommandList->ResourceBarrier(1, &uavBarrier);


    }
    transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, desc.MipLevels-1);
    appResources.computeCommandList->ResourceBarrier(1, &transition);
    transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    appResources.commandList->ResourceBarrier(1, &transition);
    SubmitComputeCommandList(appResources.computeCommandList, appResources.commandList);
    SubmitGraphicsCommandList(appResources.commandList);


}

void PrefilterLTCTexture(ComPtr<ID3D12Resource> texture)
{
    auto texWidth = texture->GetDesc().Width;
    auto texHeight = texture->GetDesc().Height;


    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    appResources.commandList->ResourceBarrier(1, &transition);

    SubmitGraphicsCommandList(appResources.commandList);

    ID3D12DescriptorHeap* ppHeaps[] = { srvUavCBVDescriptorHeap.Get() };

    appResources.computeCommandList->SetPipelineState(ltcTexPrefilterPSO.Get());
    appResources.computeCommandList->SetComputeRootSignature(ltcTexPrefilterRootSig.Get());
    appResources.computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    auto desc = texture->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = desc.Format;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    for (int i = 0; i < desc.MipLevels - 1; i++)
    {

        auto destWidth = std::max<UINT>(texWidth >> (i + 1), 1);
        auto destHeight = std::max<UINT>(texHeight >> (i + 1), 1);

        srvDesc.Texture2D.MostDetailedMip = i;
        uavDesc.Texture2D.MipSlice = i + 1 ;

        transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i);
        appResources.computeCommandList->ResourceBarrier(1, &transition);
        appResources.device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuHandle);
        cpuHandle.Offset(1, handleIncrementSize);
        lastResourceIndex++;
        appResources.device->CreateUnorderedAccessView(texture.Get(), 0, &uavDesc, cpuHandle);
        cpuHandle.Offset(1, handleIncrementSize);
        lastResourceIndex++;

        appResources.computeCommandList->SetComputeRootDescriptorTable(0, gpuHandle);
        gpuHandle.Offset(1, handleIncrementSize);
        appResources.computeCommandList->SetComputeRootDescriptorTable(1, gpuHandle);
        gpuHandle.Offset(1, handleIncrementSize);

        appResources.computeCommandList->SetComputeRoot32BitConstant(2, destWidth, 0);
        appResources.computeCommandList->SetComputeRoot32BitConstant(2, destHeight, 1);
        appResources.computeCommandList->SetComputeRoot32BitConstant(2, texWidth, 2);
        appResources.computeCommandList->SetComputeRoot32BitConstant(2, texHeight, 3);
        appResources.computeCommandList->SetComputeRoot32BitConstant(2, i, 4);


        //Dispatch the compute shader with one thread per 8x8 pixels
        appResources.computeCommandList->Dispatch(std::max<UINT>(destWidth / 8, 1u), std::max<UINT>(destHeight / 8, 1u), 1);

        // appResources.computeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        //     D3D12_RESOURCE_STATE_UNORDERED_ACCESS, i));

         //Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(texture.Get());
        appResources.computeCommandList->ResourceBarrier(1, &uavBarrier);
    }

    transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, desc.MipLevels - 1);
    appResources.commandList->ResourceBarrier(1, &transition);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    appResources.commandList->ResourceBarrier(1, &transition);

    SubmitComputeCommandList(appResources.computeCommandList, appResources.commandList);
    SubmitGraphicsCommandList(appResources.commandList);

}

void DenoiseBMFR(ManagedResource inputTex, ManagedResource inputNorm, ManagedResource inputWorld, ManagedResource inputAlbedo, ManagedResource prevNorm, ManagedResource prevWorld, ManagedResource prevAlbedo)
{
}

void BMFRPreprocess(ManagedResource rtOutput, ManagedResource normals, ManagedResource position, ManagedResource prevOutput, 
    ManagedResource prevNormals, ManagedResource prevPos, ManagedResource acceptBools, 
    ManagedResource outPrevPixelFrame, ComPtr<ID3D12Resource> cbvData, UINT frameIndex,
    ComPtr<ID3D12DescriptorHeap> srvUavHeap, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    auto commandList = appResources.commandList;


    {
        //doing the necessary copies
        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(position.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &transition);


        transition = CD3DX12_RESOURCE_BARRIER::Transition(normals.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &transition);

    }



    ID3D12DescriptorHeap* ppHeaps[] = { srvUavHeap.Get() };

    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    const float clearColor[] = { 0.4f, 0.6f, 0.75f, 0.0f };

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetPipelineState(bmfrPreProcessPSO.Get());
    commandList->SetGraphicsRootSignature(bmfrPreProcessRootSig.Get());

    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::CurPosSRV, position.srvGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::PrevPosSRV, prevPos.srvGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::CurNormSRV, normals.srvGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::PrevNormSRV, prevNormals.srvGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::CurNoisyUAV, rtOutput.uavGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::PrevNoisySRV, prevOutput.srvGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::AcceptBoolUAV, acceptBools.uavGPUHandle);
    commandList->SetGraphicsRootDescriptorTable(BMFRPreProcessRootIndices::PrevFramePixelUAV, outPrevPixelFrame.uavGPUHandle);
    commandList->SetGraphicsRootConstantBufferView(BMFRPreProcessRootIndices::FrameDataCBC, cbvData->GetGPUVirtualAddress());


    commandList->DrawInstanced(3, 1, 0, 0);

    {
        //doing the necessary copies
         auto transition = CD3DX12_RESOURCE_BARRIER::Transition(rtOutput.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(1, &transition);


         transition = CD3DX12_RESOURCE_BARRIER::Transition(prevOutput.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &transition);


        commandList->CopyResource(prevOutput.resource.Get(), rtOutput.resource.Get());


        //doing the necessary copies
         transition = CD3DX12_RESOURCE_BARRIER::Transition(rtOutput.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &transition);


         transition = CD3DX12_RESOURCE_BARRIER::Transition(prevOutput.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &transition);

        //doing the necessary copies
         transition = CD3DX12_RESOURCE_BARRIER::Transition(position.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &transition);


        transition = CD3DX12_RESOURCE_BARRIER::Transition(normals.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &transition);

    }

}

void TransitionManagedResource(const ComPtr<ID3D12GraphicsCommandList>& commandList, ManagedResource& resource, D3D12_RESOURCE_STATES afterState)
{
    if (resource.currentState == afterState)
    {
        return;
    }

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(resource.resource.Get(), resource.currentState, afterState);
    commandList->ResourceBarrier(1, &transition);

    resource.currentState = afterState;
}

void CopyResource(ComPtr<ID3D12GraphicsCommandList>& commandList, ManagedResource& dst, ManagedResource& src)
{
    auto destBefore = dst.currentState;
    auto srcBefore = src.currentState;

    TransitionManagedResource(commandList, dst, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionManagedResource(commandList, src, D3D12_RESOURCE_STATE_COPY_SOURCE);

    commandList->CopyResource(dst.resource.Get(), src.resource.Get());

    TransitionManagedResource(commandList, dst, destBefore);
    TransitionManagedResource(commandList, src, srcBefore);
}

UINT DispatchSize(UINT tgSize, UINT numElements)
{
	UINT dispatchSize = numElements / tgSize;
	dispatchSize += numElements % tgSize > 0 ? 1 : 0;
	return dispatchSize;
}


// Reads an HDR file
// Based on: http://www.cs.virginia.edu/~jcw5q/apps/imageview/src/libimageviewer/hdr.c
float* ReadHDR(const wchar_t* textureFile, unsigned int* width, unsigned int* height)
{
    const char* HeaderSignature = "#?RADIANCE";
    const char* HeaderFormat = "FORMAT=32-bit_rle_rgbe";
    const unsigned int HeaderSigSize = 10;
    const unsigned int HeaderFormatSize = 22;
    // Basic read buffer
    char buffer[1024] = { 0 };
    bool invX = false;
    bool invY = false;
    // HEADER -----------------------------------
    // Open the file
    std::fstream file(textureFile, std::ios_base::in | std::ios_base::binary);
    // Read the signature
    file.read(buffer, HeaderSigSize);
    if (strcmp(HeaderSignature, buffer) != 0)
        return nullptr;
    // Skip comment until we find FORMAT
    do {
        file.getline(buffer, sizeof(buffer));
    } while (!file.eof() && strncmp(buffer, "FORMAT", 6));
    // Did we hit the end of the file already?
    if (file.eof()) return nullptr;
    // Invalid format!
    if (strcmp(buffer, HeaderFormat) != 0)
        return nullptr;
    // Check for Y inversion
    int x = 0;
    do {
        x++;
        file.getline(buffer, sizeof(buffer));
    } while (!file.eof() && (
        strncmp(buffer, "-Y", 2) &&
        strncmp(buffer, "+Y", 2))
        );
    // End of file while looking?
    if (file.eof()) return nullptr;
    // Inverted?
    if (strncmp(buffer, "-Y", 2) == 0) invY = true;
    // Loop through buffer until X
    int counter = 0;
    while ((counter < sizeof(buffer)) && buffer[counter] != 'X')
        counter++;
    // No X?
    if (counter == sizeof(buffer)) return nullptr;
    // Flipped X?
    if (buffer[counter - 1] == '-') invX = true;
    // Grab dimensions from current buffer line
    sscanf_s(buffer, "%*s %u %*s %u", height, width);
    // Got real dimensions?
    if ((*width) == 0 || (*height) == 0)
        return nullptr;
    // ACTUAL DATA ------------------------------
    unsigned int dataSize = (*width) * (*height) * 4;
    unsigned char* data = new unsigned char[dataSize];
    memset(data, 0, dataSize); // For testing
    // Scanline encoding
    char enc[4] = { 0 };
    // Loop through the scanlines, one at a time
    for (unsigned int y = 0; y < (*height); y++)
    {
        // Inverted Y doesn't seem to matter?
        int start = /*invY ? ((*height) - y - 1) * (*width) :*/ y * (*width);
        int step = /*invX ? -1 :*/ 1;
        // Check the encoding info for this line
        file.read(enc, 4);
        if (file.eof())
            break;
        // Which RLE scheme?
        if (enc[0] == 2 && enc[1] == 2 && enc[2] >= 0)
        {
            // NEW RLE SCHEME -------------------
            // Each component is RLE'd separately
            for (int c = 0; c < 4; c++)
            {
                int pos = start;
                unsigned int x = 0;
                // Loop through the pixels/components
                while (x < (*width))
                {
                    char temp;
                    file.read(&temp, 1);
                    unsigned char num = reinterpret_cast<unsigned char&>(temp);
                    // Check if this is a run
                    if (num <= 128)
                    {
                        // No run, read the data
                        for (int i = 0; i < num; i++)
                        {
                            char temp;
                            file.read(&temp, 1);
                            data[c + pos * 4] = reinterpret_cast<unsigned char&>(temp);
                            pos += step;
                        }
                    }
                    else
                    {
                        // Run!  Get the value and set everything
                        char temp;
                        file.read(&temp, 1);
                        unsigned char value = reinterpret_cast<unsigned char&>(temp);
                        num -= 128;
                        for (int i = 0; i < num; i++)
                        {
                            data[c + pos * 4] = value;
                            pos += step;
                        }
                    }
                    // Move to the next section
                    x += num;
                }
            }
        }
        else
        {
            // OLD RLE SCHEME -------------------
            int pos = start;
            // Loop through scanline
            for (unsigned int x = 0; x < (*width); x++)
            {
                if (x > 0)
                {
                    file.read(enc, 4); // TODO: check for eof?
                }
                // Check for RLE header
                if (enc[0] == 1 && enc[1] == 1 && enc[2] == 1)
                {
                    // RLE
                    int num = ((int)enc[3]) & 0xFF;
                    unsigned char r = data[(pos - step) * 4 + 0];
                    unsigned char g = data[(pos - step) * 4 + 1];
                    unsigned char b = data[(pos - step) * 4 + 2];
                    unsigned char e = data[(pos - step) * 4 + 3];
                    // Loop and set
                    for (int i = 0; i < num; i++)
                    {
                        data[pos * 4 + 0] = r;
                        data[pos * 4 + 1] = g;
                        data[pos * 4 + 2] = b;
                        data[pos * 4 + 3] = e;
                        pos += step;
                    }
                    x += num - 1;
                }
                else
                {
                    // No RLE, just read data
                    data[pos * 4 + 0] = enc[0];
                    data[pos * 4 + 1] = enc[1];
                    data[pos * 4 + 2] = enc[2];
                    data[pos * 4 + 3] = enc[3];
                    pos += step;
                }
            }
        }
    }
    // Done with file
    file.close();
    // Convert data to final IEEE floats
    // Based on "Real Pixels" by Greg Ward in Graphics Gems II
    float* pixels = new float[(*width) * (*height) * 4];
    for (unsigned int i = 0; i < (*width) * (*height); i++)
    {
        unsigned char exponent = data[i * 4 + 3];
        if (exponent == 0)
        {
            pixels[i * 4 + 0] = 0.0f;
            pixels[i * 4 + 1] = 0.0f;
            pixels[i * 4 + 2] = 0.0f;
            pixels[i * 4 + 3] = 1.0f;
        }
        else
        {
            float v = ldexp(1.0f / 256.0f, (int)(exponent - 128));
            pixels[i * 4 + 0] = (data[i * 4 + 0] + 0.5f) * v;
            pixels[i * 4 + 1] = (data[i * 4 + 1] + 0.5f) * v;
            pixels[i * 4 + 2] = (data[i * 4 + 2] + 0.5f) * v;
            pixels[i * 4 + 3] = 1.0f;
        }
    }

    auto lol = pixels[2400];

    // Clean up
    delete[] data;
    // Success
    return pixels;
}



ApplicationResources& GetAppResources()
{
    return appResources;
}

void SubmitGraphicsCommandList(const ComPtr<ID3D12GraphicsCommandList>& commandList)
{

    commandList->Close();
    ID3D12CommandList* pcommandLists[] = { appResources.commandList.Get() };
    appResources.graphicsQueue->ExecuteCommandLists(_countof(pcommandLists), pcommandLists);
    auto lol = appResources.device->GetDeviceRemovedReason();

    appResources.commandList->Reset(appResources.commandAllocators[appResources.frameIndex].Get(), generateMipMapsPSO.Get());

    appResources.graphicsQueue->Signal(appResources.graphicsFence.Get(), appResources.fenceValues[appResources.frameIndex]);

    appResources.graphicsFence.Get()->SetEventOnCompletion(appResources.fenceValues[appResources.frameIndex], appResources.fenceEvent);
    WaitForSingleObjectEx(appResources.fenceEvent, INFINITE, false);

    appResources.fenceValues[appResources.frameIndex]++;
}

void SubmitComputeCommandList(const ComPtr<ID3D12GraphicsCommandList>& computeCommandList, const ComPtr<ID3D12GraphicsCommandList>& commandList)
{

    computeCommandList->Close();
    ID3D12CommandList* ppcommandLists[] = { computeCommandList.Get() };
    appResources.computeQueue->ExecuteCommandLists(_countof(ppcommandLists), ppcommandLists);
    ThrowIfFailed(appResources.computeQueue->Signal(appResources.computeFence.Get(), appResources.fenceValues[appResources.frameIndex]));
    ThrowIfFailed(appResources.graphicsQueue->Wait(appResources.computeFence.Get(), appResources.fenceValues[appResources.frameIndex]));
    ThrowIfFailed(computeCommandList->Reset(appResources.computeAllocator[appResources.frameIndex].Get(), generateMipMapsPSO.Get()));
}

