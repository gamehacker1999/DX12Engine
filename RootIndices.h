#pragma once
enum EntityRootIndices
{
	EntityVertexCBV,
	EntityIndex,
	EntityPixelCBV,
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