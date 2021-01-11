#include "MyModel.h"

MyModel::MyModel(std::string pathToFile)
{
	LoadModel(pathToFile);
}

void MyModel::SetMaterial(unsigned int id)
{
	matIds.emplace_back(id);
}

void MyModel::LoadModel(std::string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_MakeLeftHanded | aiProcess_CalcTangentSpace);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}

	ProcessNode(scene->mRootNode, scene);

}

void MyModel::ProcessNode(aiNode* node, const aiScene* scene)
{
	// process all the node's meshes (if any)
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(ProcessMesh(mesh, scene));
	}
	// then do the same for each of its children
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

std::shared_ptr<Mesh> MyModel::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		// process vertex positions, normals and texture coordinates
		vertex.Position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		vertex.Normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		vertex.Tangent = XMFLOAT3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
		{
			XMFLOAT2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.UV = vec;
		}
		else 
		{
			vertex.UV = XMFLOAT2(0.0f, 0.0f);  		

		}

		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	std::shared_ptr<Mesh> finalMesh = std::make_shared<Mesh>(&vertices[0], vertices.size(), &indices[0], indices.size());

	return finalMesh;
}

void MyModel::Draw(ComPtr<ID3D12GraphicsCommandList> commandList, bool drawMats)
{
	for (size_t i = 0; i < meshes.size(); i++)
	{
		if (drawMats)
		{
			unsigned int id = 0;

			if (matIds.size() >= meshes.size())
			{
				id = matIds[i];
				commandList->SetGraphicsRoot32BitConstant(EntityRootIndices::EntityMaterialIndex, id, 0);
			}

		}
		D3D12_VERTEX_BUFFER_VIEW vertexBuffer = meshes[i]->GetVertexBuffer();
		auto indexBuffer = meshes[i]->GetIndexBuffer();

		commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
		commandList->IASetIndexBuffer(&indexBuffer);

		commandList->DrawIndexedInstanced(meshes[i]->GetIndexCount(), 1, 0, 0, 0);
	}
}
