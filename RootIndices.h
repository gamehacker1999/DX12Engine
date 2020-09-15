#pragma once
enum EntityRootIndices
{
	EntityVertexCBV,
	EntityIndex,
	EntityPixelCBV,
	EntityLightListSRV,
	EntityLightIndices,
	EntityMaterials,
	EntityMaterialIndex,
	EntityEnvironmentSRV,
	EntityLTCSRV,
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
	RTDiffuseTexture,
	RTPositionTexture,
	RTNormalTexture,
	RTAlbedoTexture,
	RTAccelerationStruct,
	RTCameraData,
	RTMissTexture,
	RTMaterials,
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
	OutputMapsUAV,
	VMFFilterExternDataCBV,
	VMFFilterNumParameters
};