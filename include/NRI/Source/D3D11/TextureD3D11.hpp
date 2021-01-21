/*
Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma region [  CoreInterface  ]

static void NRI_CALL SetTextureDebugName(Texture& texture, const char* name)
{
    ((TextureD3D11&)texture).SetDebugName(name);
}

static void NRI_CALL GetTextureMemoryInfo(const Texture& texture, MemoryLocation memoryLocation, MemoryDesc& memoryDesc)
{
    ((TextureD3D11&)texture).GetMemoryInfo(memoryLocation, memoryDesc);
}

void FillFunctionTableTextureD3D11(CoreInterface& coreInterface)
{
    coreInterface.SetTextureDebugName = SetTextureDebugName;
    coreInterface.GetTextureMemoryInfo = GetTextureMemoryInfo;
}

#pragma endregion
