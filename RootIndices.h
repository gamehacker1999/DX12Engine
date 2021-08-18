#pragma once
enum EntityRootIndices
{
	EntityVertexCBV,
	EnableIndirectLighting,
	EntityPixelCBV,
	EntityLightListSRV,
	EntityLightIndices,
	EntityMaterials,
	EntityMaterialIndex,
	EntityRoughnessVMFMapSRV,
	EntityEnvironmentSRV,
	EntityLTCSRV,
	AccelerationStructureSRV,
	EntityNumRootIndices,
};

enum EnvironmentRootIndices
{
	EnvironmentVertexCBV,
	EnvironmentTextureSRV,
	EnvironmentRoughness,
	EnvironmentTexturesData,
	EnvironmentFaceIndices,
	EnvironmentNumRootIndices
};

enum OceanComputeRootIndices
{
	OceanComputeNumIndices
};

enum OceanRenderRootIndices
{
	OceanRenderNumIndices
};

enum RaytracingHeapRangesIndices
{
	RTOutputTexture,
	RTIndirectDiffuseOutputTexture,
	RTIndirectSpecularOutputTexture,
	RTRoughnessMetalTexture,
	RTPositionTexture,
	RTNormalTexture,
	RTAlbedoTexture,
	RTMotionBuffer,
	RTAccelerationStruct,
	RTCameraData,
	RTMissTexture,
	RTMaterials,
	SampleSequences,
	RTNumParameters

};

enum LightCullingRootIndices
{
	LightListSRV,
	DepthMapSRV,
	VisibleLightIndicesUAV,
	LightCullingExternalDataCBV,
	LightCullingNumParameters
};

enum VMFFilterRootIndices
{
	NormalRoughnessSRV,
	OutputRoughnessVMFUAV,
	VMFFilterExternDataCBV,
	VMFFilterNumParameters
};

enum TonemappingRootIndices
{
		
};

enum InteriorMappingRootIndices
{
	ExternDataVSCBV,
	TextureArraySRV,
	ExteriorTextureSRV,
	CapTextureSRV,
	SDFTextureSRV,
	ExternDataPSCBV,
	InteriorMappingNumParams
};

enum BMFRPreProcessRootIndices
{
	CurPosSRV,
	PrevPosSRV,
	CurNormSRV,
	PrevNormSRV,
	CurNoisyUAV,
	PrevNoisySRV,
	AcceptBoolUAV,
	PrevFramePixelUAV,
	FrameDataCBC,
	BMFRPreProcessNumParams
};

enum BMFRRegressionRootIndices
{
	CurPositionSRV,
	CurNormalSRV,
	CurAlbedoSRV,
	CurrentNoisyUAV,
	TempDataUAV,
	OutDataUAV,
	FrameDataCBV,
	BMFRRegressionNumParams
};

enum BMFRPostProcessRootIndices
{
	FilterFrameSRV,
	AccumPrevFrameSRV,
	AlbedoSRV,
	AcceptBoolsSRV,
	PrevFramePixelSRV,
	AccumFrameUAV,
	FrameDataConstants,
	BMFRPostProcessNumParams
};

enum BlueNoiseDithering
{
	BlueNoiseTex,
	RetargetTex,
	PrevFrameNoisy,
	NewSequences,
	RetargettedSequencesBNDS,
	FrameNum, 
	BNDSNumParams
};

enum RetargetingPass
{
	RetargetTexture,
	OldSequences,
	RetargetedSequences,
	RetargetingPassCBV,
	RetargetingPassNumParams
};

enum BilateralBlur
{
	MainColorTex,
	DepthTexBlur,
	BilateralBlurExternalData,
	BilateralBlurNumParams
};

enum RestrirSpatialReuseIndices
{
	RestirSpatialReuse_GBufferPos,
	RestirSpatialReuse_GBufferNorm,
	RestirSpatialReuse_GBufferDif,
	RestirSpatialReuse_GBufferRoughMetal,
	RestirSpatialReuse_OutColor,
	RestirSpatialReuse_Reservoirs,
	RestirSpatialReuse_SampleSequences,
	RestirSpatialReuse_OutReservoirs,
	RestirSpatialReuse_ExternData,
	RestirSpatialReuse_Lights,
	RestirSpatialReuse_AccelStruct,
	RestirSpatialReuse_NumIndices
};