#pragma once
#include "Material.h"
class InteriorMaterial :
    public Material
{
    ManagedResource* interiorMaps = nullptr;
    ManagedResource textureArray;
    ManagedResource externalTexture;
    ManagedResource capTexture;
    ManagedResource sdfTexture;
    DescriptorHeapWrapper textureArrayHeap;
public:
    InteriorMaterial(int numTextures = 5);
    ~InteriorMaterial();

    DescriptorHeapWrapper& GetDescriptorHeap();
    ManagedResource& GetExternalTexture();
};

