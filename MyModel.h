#pragma once
#include "DX12Helper.h"
#include"Mesh.h"
#include <memory>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <iostream>
class MyModel
{
public:

	MyModel(std::string pathToFile);
    void SetMaterial(unsigned int id);
    void Draw(ComPtr<ID3D12GraphicsCommandList> commandList, bool drawMats = true);
    std::vector<std::shared_ptr<Mesh>> GetMeshes();

private:
	std::vector<std::shared_ptr<Mesh>> meshes;
    UINT lastMeshID;

    //material ids for the meshes
    std::vector<unsigned int> matIds;

    void LoadModel(std::string path);
    void ProcessNode(aiNode* node, const aiScene* scene);
    std::shared_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
 
   // std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type,
   //     string typeName);
};

