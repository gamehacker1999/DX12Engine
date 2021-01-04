//#include "Renderer.h"
//
//
//void Renderer::LoadShaders()
//{
//
//	//this describes the type of constant buffer and which register to map the data to
//	CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
//	CD3DX12_ROOT_PARAMETER1 rootParams[EntityRootIndices::EntityNumRootIndices]; // specifies the descriptor table
//	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
//	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
//	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
//	ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
//	ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 3);
//	rootParams[EntityRootIndices::EntityVertexCBV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
//	rootParams[EntityRootIndices::EntityIndex].InitAsConstants(1, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityPixelCBV].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityMaterials].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityRoughnessVMFMapSRV].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityLightListSRV].InitAsShaderResourceView(0, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityLightIndices].InitAsShaderResourceView(1, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityMaterialIndex].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityEnvironmentSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EntityRootIndices::EntityLTCSRV].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
//
//	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];//(0, D3D12_FILTER_ANISOTROPIC);
//	staticSamplers[0].Init(0);
//	staticSamplers[1].Init(1, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
//
//	//rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
//
//
//	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
//		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
//
//	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
//	rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers, rootSignatureFlags);
//
//	ComPtr<ID3DBlob> signature;
//	ComPtr<ID3DBlob> error;
//
//	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
//		signature.GetAddressOf(), error.GetAddressOf()));
//	//if (FAILED(hr)) return hr;
//
//	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
//		IID_PPV_ARGS(rootSignature.GetAddressOf())));
//
//	//if (FAILED(hr)) return hr;
//	ComPtr<ID3DBlob> vertexShaderBlob;
//	ComPtr<ID3DBlob> pixelShaderBlob;
//	ComPtr<ID3DBlob> pbrPixelShaderBlob;
//	ComPtr<ID3DBlob> sssPixelShaderBlob;
//	ComPtr<ID3DBlob> depthPrePassPSBlob;
//	ComPtr<ID3DBlob> depthPrePassVSBlob;
//	//load shaders
//	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"PixelShaderPBR.cso", pbrPixelShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"SubsurfaceScatteringPS.cso", sssPixelShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"DepthPrePassPS.cso", depthPrePassPSBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"DepthPrePassVS.cso", depthPrePassVSBlob.GetAddressOf()));
//
//	//input vertex layout, describes the semantics
//
//	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
//	{
//		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//
//	};
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
//	psoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	psoDesc.pRootSignature = rootSignature.Get();
//	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
//	////psoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//psoDesc.DepthStencilState.StencilEnable = FALSE;
//	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	psoDesc.SampleMask = UINT_MAX;
//	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	psoDesc.NumRenderTargets = 1;
//	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	psoDesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescPBR = {};
//	psoDescPBR.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	psoDescPBR.pRootSignature = rootSignature.Get();
//	psoDescPBR.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	psoDescPBR.PS = CD3DX12_SHADER_BYTECODE(pbrPixelShaderBlob.Get());
//	////psoPBRDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//psoDePBRsc.DepthStencilState.StencilEnable = FALSE;
//	psoDescPBR.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	psoDescPBR.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	psoDescPBR.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	psoDescPBR.SampleMask = UINT_MAX;
//	psoDescPBR.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	psoDescPBR.NumRenderTargets = 1;
//	psoDescPBR.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	psoDescPBR.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	psoDescPBR.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescPBR, IID_PPV_ARGS(pbrPipelineState.GetAddressOf())));
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC sssDescPBR = {};
//	sssDescPBR.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	sssDescPBR.pRootSignature = rootSignature.Get();
//	sssDescPBR.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	sssDescPBR.PS = CD3DX12_SHADER_BYTECODE(sssPixelShaderBlob.Get());
//	sssDescPBR.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	sssDescPBR.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	sssDescPBR.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	sssDescPBR.SampleMask = UINT_MAX;
//	sssDescPBR.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	sssDescPBR.NumRenderTargets = 1;
//	sssDescPBR.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	sssDescPBR.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	sssDescPBR.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&sssDescPBR, IID_PPV_ARGS(sssPipelineState.GetAddressOf())));
//
//	//creating a depth prepass pipeline state
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC depthPrePassPSODesc = {};
//	depthPrePassPSODesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	depthPrePassPSODesc.pRootSignature = rootSignature.Get();
//	depthPrePassPSODesc.VS = CD3DX12_SHADER_BYTECODE(depthPrePassVSBlob.Get());
//	depthPrePassPSODesc.PS = CD3DX12_SHADER_BYTECODE(depthPrePassPSBlob.Get());
//	depthPrePassPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	depthPrePassPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	depthPrePassPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	depthPrePassPSODesc.SampleMask = UINT_MAX;
//	depthPrePassPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	depthPrePassPSODesc.NumRenderTargets = 0;
//	depthPrePassPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	depthPrePassPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
//	depthPrePassPSODesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&depthPrePassPSODesc, IID_PPV_ARGS(depthPrePassPipelineState.GetAddressOf())));
//
//
//	CD3DX12_DESCRIPTOR_RANGE1 volumeRanges[1];
//	CD3DX12_ROOT_PARAMETER1 volumeRootParams[2];
//
//	volumeRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
//
//	volumeRootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL);
//	volumeRootParams[1].InitAsDescriptorTable(1, &volumeRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
//
//	ComPtr<ID3DBlob> volumeSignature;
//	ComPtr<ID3DBlob> volumeError;
//
//	CD3DX12_STATIC_SAMPLER_DESC staticSamplersVolume[1];//(0, D3D12_FILTER_ANISOTROPIC);
//	staticSamplersVolume[0].Init(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
//
//	rootSignatureDesc.Init_1_1(_countof(volumeRootParams), volumeRootParams, _countof(staticSamplersVolume), staticSamplersVolume, rootSignatureFlags);
//
//	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, volumeSignature.GetAddressOf(), volumeError.GetAddressOf()));
//
//	ThrowIfFailed(device->CreateRootSignature(0, volumeSignature->GetBufferPointer(), volumeSignature->GetBufferSize(), IID_PPV_ARGS(volumeRootSignature.GetAddressOf())));
//
//	ComPtr<ID3DBlob> rayMarchedVolumeVS;
//	ComPtr<ID3DBlob> raymarcedVolumePS;
//
//	ThrowIfFailed(D3DReadFileToBlob(L"VolumeRayMarcherPS.cso", raymarcedVolumePS.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"VolumeRayMarcherVS.cso", rayMarchedVolumeVS.GetAddressOf()));
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescVolume = {};
//	psoDescVolume.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	psoDescVolume.pRootSignature = volumeRootSignature.Get();
//	psoDescVolume.VS = CD3DX12_SHADER_BYTECODE(rayMarchedVolumeVS.Get());
//	psoDescVolume.PS = CD3DX12_SHADER_BYTECODE(raymarcedVolumePS.Get());
//	////psoPBRDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//psoDePBRsc.DepthStencilState.StencilEnable = FALSE;
//	psoDescVolume.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	psoDescVolume.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	psoDescVolume.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state
//	psoDescVolume.BlendState.AlphaToCoverageEnable = false;
//	psoDescVolume.BlendState.IndependentBlendEnable = false;
//	psoDescVolume.BlendState.RenderTarget[0].BlendEnable = true;
//	psoDescVolume.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
//	psoDescVolume.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_COLOR;
//	//psoDescVolume.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
//	psoDescVolume.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
//	psoDescVolume.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
//	psoDescVolume.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
//	psoDescVolume.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
//	psoDescVolume.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
//	psoDescVolume.SampleMask = UINT_MAX;
//	psoDescVolume.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	psoDescVolume.NumRenderTargets = 1;
//	psoDescVolume.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	psoDescVolume.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	psoDescVolume.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescVolume, IID_PPV_ARGS(volumePSO.GetAddressOf())));
//
//	//creating particle root sig and pso
//	CD3DX12_DESCRIPTOR_RANGE1 particleDescriptorRange[2];
//	CD3DX12_ROOT_PARAMETER1 particleRootParams[3];
//
//	//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
//	particleDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
//
//	//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
//	particleRootParams[0].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
//	//
//	particleRootParams[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
//	particleRootParams[2].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
//
//	ComPtr<ID3DBlob> particleSignature;
//	ComPtr<ID3DBlob> particleError;
//
//	CD3DX12_STATIC_SAMPLER_DESC staticSamplersParticle[1];//(0, D3D12_FILTER_ANISOTROPIC);
//	staticSamplersParticle[0].Init(0);
//
//	rootSignatureDesc.Init_1_1(_countof(particleRootParams), particleRootParams,
//		_countof(staticSamplersParticle), staticSamplersParticle, rootSignatureFlags);
//
//	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
//		particleSignature.GetAddressOf(), particleError.GetAddressOf()));
//
//	ThrowIfFailed(device->CreateRootSignature(0, particleSignature->GetBufferPointer(), particleSignature->GetBufferSize(),
//		IID_PPV_ARGS(particleRootSig.GetAddressOf())));
//
//	ComPtr<ID3DBlob> particleVS;
//	ComPtr<ID3DBlob> particlePS;
//
//	ThrowIfFailed(D3DReadFileToBlob(L"ParticlePS.cso", particlePS.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"ParticleVS.cso", particleVS.GetAddressOf()));
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescParticle = {};
//	//psoDescParticle.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	psoDescParticle.pRootSignature = particleRootSig.Get();
//	psoDescParticle.VS = CD3DX12_SHADER_BYTECODE(particleVS.Get());
//	psoDescParticle.PS = CD3DX12_SHADER_BYTECODE(particlePS.Get());
//	//psoDescParticle.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	psoDescParticle.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
//	psoDescParticle.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
//	psoDescParticle.DepthStencilState.DepthEnable = true;
//	psoDescParticle.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	psoDescParticle.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
//	psoDescParticle.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state
//	psoDescParticle.BlendState.AlphaToCoverageEnable = false;
//	psoDescParticle.BlendState.IndependentBlendEnable = false;
//	psoDescParticle.BlendState.RenderTarget[0].BlendEnable = true;
//	psoDescParticle.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
//	psoDescParticle.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
//	psoDescParticle.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
//	psoDescParticle.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
//	psoDescParticle.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
//	psoDescParticle.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
//	psoDescParticle.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
//	psoDescParticle.SampleMask = UINT_MAX;
//	psoDescParticle.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	psoDescParticle.NumRenderTargets = 1;
//	psoDescParticle.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	psoDescParticle.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	psoDescParticle.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescParticle, IID_PPV_ARGS(particlesPSO.GetAddressOf())));
//
//	//Light culling setup
//	{
//
//		//Creating the light culling root signature and pipeline state
//		CD3DX12_DESCRIPTOR_RANGE1 computeRootRanges[1];
//		CD3DX12_ROOT_PARAMETER1 lightCullingRootParams[LightCullingNumParameters];
//		computeRootRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
//		lightCullingRootParams[LightCullingRootIndices::LightListSRV].InitAsShaderResourceView(0, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
//		lightCullingRootParams[LightCullingRootIndices::DepthMapSRV].InitAsDescriptorTable(1, &computeRootRanges[0]);
//		lightCullingRootParams[LightCullingRootIndices::VisibleLightIndicesUAV].InitAsUnorderedAccessView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);
//		lightCullingRootParams[LightCullingRootIndices::LightCullingExternalDataCBV].InitAsConstantBufferView(0, 0);
//
//
//		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
//		computeRootSignatureDesc.Init_1_1(_countof(lightCullingRootParams), lightCullingRootParams);
//
//		ComPtr<ID3DBlob> computeSignature;
//		ComPtr<ID3DBlob> computeError;
//
//		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
//		ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)));
//
//		ComPtr<ID3DBlob> lightCullingCS;
//
//		ThrowIfFailed(D3DReadFileToBlob(L"LightCullingCS.cso", lightCullingCS.GetAddressOf()));
//
//		D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
//		computePSODesc.pRootSignature = computeRootSignature.Get();
//		computePSODesc.CS = CD3DX12_SHADER_BYTECODE(lightCullingCS.Get());
//
//		ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(computePipelineState.GetAddressOf())));
//	}
//
//	//vmf solver set up
//	{
//		CD3DX12_ROOT_PARAMETER1 rootParams[VMFFilterRootIndices::VMFFilterNumParameters + 1];
//		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
//		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);
//		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
//		rootParams[VMFFilterRootIndices::OutputRoughnessVMFUAV].InitAsDescriptorTable(1, &ranges[1]);
//		rootParams[VMFFilterRootIndices::NormalRoughnessSRV].InitAsDescriptorTable(1, &ranges[0]);
//		rootParams[VMFFilterRootIndices::VMFFilterExternDataCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);
//		rootParams[VMFFilterNumParameters].InitAsConstants(1, 1, 0);
//
//		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
//		computeRootSignatureDesc.Init_1_1(_countof(rootParams), rootParams);
//
//		ComPtr<ID3DBlob> computeSignature;
//		ComPtr<ID3DBlob> computeError;
//
//		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &computeSignature, &computeError));
//		ThrowIfFailed(device->CreateRootSignature(0, computeSignature->GetBufferPointer(), computeSignature->GetBufferSize(), IID_PPV_ARGS(&vmfSofverRootSignature)));
//
//		ComPtr<ID3DBlob> vmfSolverBlob;
//		ThrowIfFailed(D3DReadFileToBlob(L"VMFSolverCS.cso", vmfSolverBlob.GetAddressOf()));
//
//		D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
//		computePSODesc.pRootSignature = vmfSofverRootSignature.Get();
//		computePSODesc.CS = CD3DX12_SHADER_BYTECODE(vmfSolverBlob.Get());
//
//		ThrowIfFailed(device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(vmfSolverPSO.GetAddressOf())));
//
//	}
//
//	//setting up post processing shaders
//	{
//
//		//creating particle root sig and pso
//		CD3DX12_DESCRIPTOR_RANGE1 toneMappingDescriptorRange[1];
//		CD3DX12_ROOT_PARAMETER1 tonemappingRootParams[2];
//
//		//particleDescriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
//		toneMappingDescriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
//
//		//particleRootParams[0].InitAsDescriptorTable(1, &particleDescriptorRange[0], D3D12_SHADER_VISIBILITY_VERTEX);
//		tonemappingRootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);
//		tonemappingRootParams[1].InitAsDescriptorTable(1, &toneMappingDescriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
//
//		ComPtr<ID3DBlob> tonemappingSignature;
//		ComPtr<ID3DBlob> tonemappingError;
//
//		CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
//		staticSamplers[0].Init(0);
//
//		rootSignatureDesc.Init_1_1(_countof(tonemappingRootParams), tonemappingRootParams,
//			_countof(staticSamplers), staticSamplers, rootSignatureFlags);
//
//		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
//			tonemappingSignature.GetAddressOf(), tonemappingError.GetAddressOf()));
//
//		ThrowIfFailed(device->CreateRootSignature(0, tonemappingSignature->GetBufferPointer(), tonemappingSignature->GetBufferSize(),
//			IID_PPV_ARGS(toneMappingRootSig.GetAddressOf())));
//
//		ComPtr<ID3DBlob> fullcreenVS;
//		ComPtr<ID3DBlob> tonemappingPS;
//
//		ThrowIfFailed(D3DReadFileToBlob(L"TonemappingPS.cso", tonemappingPS.GetAddressOf()));
//		ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", fullcreenVS.GetAddressOf()));
//
//		//creating a tonemapping pipeline state
//		D3D12_GRAPHICS_PIPELINE_STATE_DESC toneMappingPSODesc = {};
//		toneMappingPSODesc.InputLayout = { };
//		toneMappingPSODesc.pRootSignature = toneMappingRootSig.Get();
//		toneMappingPSODesc.VS = CD3DX12_SHADER_BYTECODE(fullcreenVS.Get());
//		toneMappingPSODesc.PS = CD3DX12_SHADER_BYTECODE(tonemappingPS.Get());
//		toneMappingPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//		toneMappingPSODesc.DepthStencilState.DepthEnable = false;
//		toneMappingPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//		toneMappingPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//		toneMappingPSODesc.SampleMask = UINT_MAX;
//		toneMappingPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//		toneMappingPSODesc.NumRenderTargets = 1;
//		toneMappingPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//		toneMappingPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
//		toneMappingPSODesc.SampleDesc.Count = 1;
//		ThrowIfFailed(device->CreateGraphicsPipelineState(&toneMappingPSODesc, IID_PPV_ARGS(toneMappingPSO.GetAddressOf())));
//	}
//}
//
//void Renderer::CreateEnvironment(std::wstring skyboxLoc)
//{
//	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
//	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
//	CD3DX12_ROOT_PARAMETER1 rootParams[EnvironmentRootIndices::EnvironmentNumRootIndices]; // specifies the descriptor table
//	rootParams[EnvironmentRootIndices::EnvironmentVertexCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
//	rootParams[EnvironmentRootIndices::EnvironmentTextureSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EnvironmentRootIndices::EnvironmentRoughness].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
//	rootParams[EnvironmentRootIndices::EnvironmentTexturesData].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);
//	rootParams[EnvironmentRootIndices::EnvironmentFaceIndices].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
//
//
//	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];//(0, D3D12_FILTER_ANISOTROPIC);
//	staticSamplers[0].Init(0);
//
//	//rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
//
//	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
//		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
//		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
//
//	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
//	rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, _countof(staticSamplers), staticSamplers, rootSignatureFlags);
//
//	ComPtr<ID3DBlob> signature;
//	ComPtr<ID3DBlob> error;
//
//	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
//		signature.GetAddressOf(), error.GetAddressOf()));
//	//if (FAILED(hr)) return hr;
//
//	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
//		IID_PPV_ARGS(skyboxRootSignature.GetAddressOf())));
//
//	//if (FAILED(hr)) return hr;
//	ComPtr<ID3DBlob> vertexShaderBlob;
//	ComPtr<ID3DBlob> pixelShaderBlob;
//	//load shaders
//	ThrowIfFailed(D3DReadFileToBlob(L"CubeMapVS.cso", vertexShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"CubeMapPS.cso", pixelShaderBlob.GetAddressOf()));
//
//	//input vertex layout, describes the semantics
//
//	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
//	{
//		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//
//	};
//
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
//	psoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	psoDesc.pRootSignature = skyboxRootSignature.Get();
//	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
//	////psoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//psoDesc.DepthStencilState.StencilEnable = FALSE;
//	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
//	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
//	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
//	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	psoDesc.SampleMask = UINT_MAX;
//	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	psoDesc.NumRenderTargets = 1;
//	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	psoDesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(skyboxPSO.GetAddressOf())));
//
//	//creating the skybox
//	skybox = std::make_shared<Skybox>(skyboxLoc, skyboxPSO, skyboxRootSignature, false);
//
//	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleAllocator.Get(), skyboxPSO.Get(), IID_PPV_ARGS(skyboxBundle.GetAddressOf())));
//
//	//loading the shaders for image based lighting
//
//	//irradiance map calculations
//
//
//	//load shaders
//	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"IrradianceMapPS.cso", pixelShaderBlob.GetAddressOf()));
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC irradiancePsoDesc = {};
//	irradiancePsoDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	irradiancePsoDesc.pRootSignature = skyboxRootSignature.Get();
//	irradiancePsoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	irradiancePsoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
//	//irradiancePsoDescDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//irradiancePsoDescsc.DepthStencilState.StencilEnable = FALSE;
//	irradiancePsoDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	irradiancePsoDesc.DepthStencilState.StencilEnable = FALSE;
//	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	irradiancePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	irradiancePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	irradiancePsoDesc.SampleMask = UINT_MAX;
//	irradiancePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	irradiancePsoDesc.NumRenderTargets = 1;
//	irradiancePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
//	irradiancePsoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	irradiancePsoDesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&irradiancePsoDesc, IID_PPV_ARGS(irradiencePSO.GetAddressOf())));
//
//	//prefilteredmap
//	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"PrefilteredMapPS.cso", pixelShaderBlob.GetAddressOf()));
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC prefiltermapPSODesc = {};
//	prefiltermapPSODesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	prefiltermapPSODesc.pRootSignature = skyboxRootSignature.Get();
//	prefiltermapPSODesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	prefiltermapPSODesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
//	//prefiltermapPSODescscDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	//prefiltermapPSODescscsc.DepthStencilState.StencilEnable = FALSE;
//	prefiltermapPSODesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	prefiltermapPSODesc.DepthStencilState.StencilEnable = FALSE;
//	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	prefiltermapPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	prefiltermapPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	prefiltermapPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	prefiltermapPSODesc.SampleMask = UINT_MAX;
//	prefiltermapPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	prefiltermapPSODesc.NumRenderTargets = 1;
//	prefiltermapPSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
//	prefiltermapPSODesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	prefiltermapPSODesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&prefiltermapPSODesc, IID_PPV_ARGS(prefilteredMapPSO.GetAddressOf())));
//
//	//BRDF LUT
//
//	ThrowIfFailed(D3DReadFileToBlob(L"FullScreenTriangleVS.cso", vertexShaderBlob.GetAddressOf()));
//	ThrowIfFailed(D3DReadFileToBlob(L"IntegrationBrdfPS.cso", pixelShaderBlob.GetAddressOf()));
//	//creating a pipeline state object
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC integrationBRDFDesc = {};
//	integrationBRDFDesc.InputLayout = { inputElementDesc,_countof(inputElementDesc) };
//	integrationBRDFDesc.pRootSignature = skyboxRootSignature.Get();
//	integrationBRDFDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
//	integrationBRDFDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
//	integrationBRDFDesc.DepthStencilState.DepthEnable = FALSE; //= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
//	integrationBRDFDesc.DepthStencilState.StencilEnable = FALSE;
//	//integrationBRDFDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//	integrationBRDFDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
//	integrationBRDFDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
//	integrationBRDFDesc.SampleMask = UINT_MAX;
//	integrationBRDFDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	integrationBRDFDesc.NumRenderTargets = 1;
//	integrationBRDFDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
//	integrationBRDFDesc.RTVFormats[0] = DXGI_FORMAT_R32G32_FLOAT;
//	integrationBRDFDesc.SampleDesc.Count = 1;
//	ThrowIfFailed(device->CreateGraphicsPipelineState(&integrationBRDFDesc, IID_PPV_ARGS(brdfLUTPSO.GetAddressOf())));
//
//}
//
//std::unique_ptr<Renderer>& Renderer::GetInstance()
//{
//	if (instanceFlag == false)
//	{
//		renderer = std::make_unique<Renderer>();
//	}
//
//	return renderer;
//}
//
//HRESULT Renderer::Init()
//{
//	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
//	ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
//		&options5, sizeof(options5)));
//	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
//		isRaytracingAllowed = false;
//	else
//		isRaytracingAllowed = true;
//
//	frameIndex = this->swapChain->GetCurrentBackBufferIndex();
//
//	HRESULT hr;
//
//	// Create descriptor heaps.
//	{
//
//		ThrowIfFailed(rtvDescriptorHeap.Create(device, frameCount + 1, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
//
//		ThrowIfFailed(dsDescriptorHeap.Create(device, 2, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
//
//	}
//
//
//	// Create frame resources.
//	{
//
//		// Create a RTV for each frame.
//		for (UINT n = 0; n < frameCount; n++)
//		{
//			hr = (this->swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n].resource)));
//			if (FAILED(hr)) return hr;
//			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
//			device->CreateRenderTargetView(renderTargets[n].resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(n));
//			rtvDescriptorHeap.IncrementLastResourceIndex(1);
//			//rtvHandle.Offset(1, rtvDescriptorSize);
//
//			hr = (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
//			if (FAILED(hr)) return hr;
//
//			//creating the compute command allocators
//			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(computeCommandAllocator[n].GetAddressOf())));
//		}
//
//	}
//
//	InitComputeEngine();
//
//
//	//create synchronization object and wait till the objects have been passed to the gpu
//	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
//	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(computeFence.GetAddressOf())));
//	fenceValues[frameIndex]++;
//	//fence event handle for synchronization
//	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
//
//	if (fenceEvent == nullptr)
//	{
//		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
//	}
//
//	//create command list
//	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), pipelineState.Get(),
//		IID_PPV_ARGS(commandList.GetAddressOf())));
//	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, computeCommandAllocator[frameIndex].Get(), computePipelineState.Get(), IID_PPV_ARGS(&computeCommandList)));
//
//	InitResources(device, commandList, computeCommandList, commandQueue, commandAllocators,
//		computeCommandQueue, computeCommandAllocator, fence, computeFence, fenceValues, fenceEvent);
//
//
//
//	gpuHeapRingBuffer = std::make_shared<GPUHeapRingBuffer>();
//
//	//residencyManager = std::make_shared<D3DX12Residency::ResidencyManager>();
//	residencyManager.Initialize(device.Get(), 0, adapter.Get(), frameCount);
//	residencySet = std::shared_ptr<D3DX12Residency::ResidencySet>(residencyManager.CreateResidencySet());
//
//	residencySet->Open();
//
//	sceneConstantBufferAlignmentSize = (sizeof(SceneConstantBuffer));
//
//
//	//creating a final render target
//	D3D12_RESOURCE_DESC renderTexureDesc = {};
//	renderTexureDesc.Width = width;
//	renderTexureDesc.Height = height;
//	renderTexureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
//	renderTexureDesc.DepthOrArraySize = renderTargets[0].resource->GetDesc().DepthOrArraySize;
//	renderTexureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
//	renderTexureDesc.MipLevels = renderTargets[0].resource->GetDesc().MipLevels;
//	renderTexureDesc.SampleDesc.Quality = 0;
//	renderTexureDesc.SampleDesc.Count = 1;
//	renderTexureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//
//	// Background color (Cornflower Blue in this case) for clearing
//	FLOAT color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };
//
//	D3D12_CLEAR_VALUE rtvClearVal = {};
//	rtvClearVal.Color[0] = color[0];
//	rtvClearVal.Color[1] = color[1];
//	rtvClearVal.Color[2] = color[2];
//	rtvClearVal.Color[3] = color[3];
//	rtvClearVal.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
//
//
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE,
//		&renderTexureDesc,
//		D3D12_RESOURCE_STATE_COPY_SOURCE,
//		&rtvClearVal,
//		IID_PPV_ARGS(finalRenderTarget.resource.GetAddressOf())
//	));
//
//
//
//	device->CreateRenderTargetView(finalRenderTarget.resource.Get(), nullptr, rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex()));
//	finalRenderTarget.rtvCPUHandle = rtvDescriptorHeap.GetCPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
//	finalRenderTarget.rtvGPUHandle = rtvDescriptorHeap.GetGPUHandle(rtvDescriptorHeap.GetLastResourceIndex());
//	rtvDescriptorHeap.IncrementLastResourceIndex(1);
//
//	renderTargetSRVHeap.Create(device, 3, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//	renderTargetSRVHeap.CreateDescriptor(finalRenderTarget, RESOURCE_TYPE_SRV, device, 0, width, height, 0, 1);
//
//
//	//optimized clear value for depth stencil buffer
//	D3D12_CLEAR_VALUE depthClearValue = {};
//	depthClearValue.DepthStencil.Depth = 1.0f;
//	depthClearValue.DepthStencil.Stencil = 0;
//	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
//
//	//creating the default resource heap for the depth stencil
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
//		D3D12_RESOURCE_STATE_DEPTH_WRITE,
//		&depthClearValue,
//		IID_PPV_ARGS(depthStencilBuffer.resource.GetAddressOf())
//	));
//
//	dsDescriptorHeap.CreateDescriptor(depthStencilBuffer, RESOURCE_TYPE_DSV, device, 0, width, height);
//
//	D3D12_RESOURCE_DESC depthTexDesc = {};
//	depthTexDesc.Width = width;
//	depthTexDesc.Height = height;
//	depthTexDesc.DepthOrArraySize = 1;
//	depthTexDesc.MipLevels = 1;
//	depthTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//	depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
//	depthTexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
//	depthTexDesc.SampleDesc.Count = 1;
//	depthTexDesc.SampleDesc.Quality = 0;
//	depthTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
//	depthTexDesc.Alignment = 0;
//
//	//creating the default resource heap for the depth stencil
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE,
//		&depthTexDesc,
//		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
//		&depthClearValue,
//		IID_PPV_ARGS(depthTex.resource.GetAddressOf())
//	));
//
//	depthTex.currentState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
//	dsDescriptorHeap.CreateDescriptor(depthTex, RESOURCE_TYPE_DSV, device, 0, width, height);
//
//	depthDesc.Create(device, 2, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//	depthDesc.CreateDescriptor(depthTex, RESOURCE_TYPE_SRV, device, 0, 0, 0, 0, 1);
//
//
//	//creating the skybox bundle
//	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(bundleAllocator.GetAddressOf())));
//
//	//memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
//	//memcpy(constantBufferBegin+sceneConstantBufferAlignmentSize, &constantBufferData, sizeof(constantBufferData));
//
//
//	// Helper methods for loading shaders, creating some basic
//
//		//creatng the constant buffer heap before creating the entity
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer) * 3),
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(cbufferUploadHeap.GetAddressOf())
//	));
//
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(lightConstantBufferResource.GetAddressOf())
//	));
//
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(lightingConstantBufferResource.GetAddressOf())
//	));
//
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(lightCullingCBVResource.GetAddressOf())
//	));
//
//	ZeroMemory(&lightData, sizeof(lightData));
//	ZeroMemory(&lightingData, sizeof(lightingData));
//
//
//	//creating the light list srv
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Light)* MAX_LIGHTS),
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(lightListResource.GetAddressOf())
//	));
//
//	int workGroupsX = (width + (width % TILE_SIZE)) / TILE_SIZE;
//	int workGroupsY = (height + (height % TILE_SIZE)) / TILE_SIZE;
//	size_t numberOfTiles = workGroupsX * workGroupsY;
//
//	visibleLightIndices = new UINT[workGroupsX * workGroupsY * 1024];
//
//	ThrowIfFailed(device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE,
//		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * 1024 * numberOfTiles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
//		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
//		nullptr,
//		IID_PPV_ARGS(visibleLightIndicesBuffer.resource.GetAddressOf())
//	));
//
//	visibleLightIndicesBuffer.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
//
//	lights = new Light[MAX_LIGHTS];
//
//	ZeroMemory(lights, MAX_LIGHTS * sizeof(Light));
//
//	lightConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightCbufferBegin));
//	memcpy(lightCbufferBegin, &lightData, sizeof(lightData));
//
//	lightingData.lightCount = lightCount;
//
//	lightingConstantBufferResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightingCbufferBegin));
//	memcpy(lightingCbufferBegin, &lightingData, sizeof(lightingData));
//
//	lightCullingCBVResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightCullingExternBegin));
//
//
//	lightListResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&lightBufferBegin));
//	memcpy(lightBufferBegin, lights, MAX_LIGHTS * sizeof(Light));
//
//	LoadShaders();
//}
//