#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
#include<memory>
#include"Model.h"
#include"Vertex.h"
#include<string>
#include<vector>
#include<fstream>
using namespace DirectX;

class Game;
class Mesh
{
	//buffers to hold vertex and index buffers
	D3D12_VERTEX_BUFFER_VIEW vertexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBuffer;

	ComPtr<ID3D12Resource> defaultHeap;
	ComPtr<ID3D12Resource> uploadHeap;

	ComPtr<ID3D12Resource> defaultIndexHeap;
	ComPtr<ID3D12Resource> uploadIndexHeap;

	unsigned int numIndices; //number of indices in the mesh
	unsigned int numVertices; //number of indices in the mesh
	std::vector<Vector3> points;

	//list of vertices and indices
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	UINT materialID;

public:
	Mesh(std::vector<Vertex> vertices, unsigned int numVertices, std::vector<UINT> indices, int numIndices, ComPtr<ID3D12Device> device = nullptr , ComPtr<ID3D12GraphicsCommandList> commandList = nullptr, ComPtr<ID3D12CommandQueue> commandQueue = nullptr);
	Mesh(std::string fileName, ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue = nullptr);
	void CalculateTangents(std::vector<Vertex>& vertices, std::vector<Vector3>& position,
		std::vector<Vector2>& uvs, unsigned int vertCount);
	~Mesh();

	std::pair<ComPtr<ID3D12Resource>, UINT> GetVertexBufferResourceAndCount();

	D3D12_VERTEX_BUFFER_VIEW GetVertexBuffer();
	D3D12_INDEX_BUFFER_VIEW GetIndexBuffer();
	ComPtr<ID3D12Resource> GetVertexBufferResource();
	unsigned int GetIndexCount();
	unsigned int GetVertexCount();
	std::vector<Vector3> GetPoints();
	std::vector<Vertex> GetVerts();

	bool RayMeshTest(Vector4 origin, Vector4 direction);

	//load fbx files
	void LoadFBX(ComPtr<ID3D12Device> device , std::string& filename);
	//method to load obj files
	void LoadOBJ(ComPtr<ID3D12Device> device , std::string& fileName, ComPtr<ID3D12GraphicsCommandList> commandList);
	//sdk meshes
	void LoadSDKMesh(ComPtr<ID3D12Device> device, std::string& fileName, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue);

	void SetMaterialID(UINT id);

	UINT GetMaterialID();

	//function to load draw the mesh
	//void Draw(ID3D11DeviceContext* context);
};

