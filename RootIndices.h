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
	RTDiffuseTexture,
	RTPositionTexture,
	RTNormalTexture,
	RTAlbedoTexture,
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
	PrevFrameNoisy,
	NewSequences,
	FrameNum, 
	BNDSNumParams
};