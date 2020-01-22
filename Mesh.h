#pragma once
#include"DX12Helper.h"
#include<DirectXMath.h>
#include<memory>
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
	std::vector<XMFLOAT3> points;

public:
	Mesh(Vertex* vertices, unsigned int numVertices, unsigned int* indices, int numIndices, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList);
	Mesh(std::string fileName, ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> commandList);
	void CalculateTangents(std::vector<Vertex>& vertices, std::vector<XMFLOAT3>& position,
		std::vector<XMFLOAT2>& uvs, unsigned int vertCount);
	~Mesh();

	D3D12_VERTEX_BUFFER_VIEW GetVertexBuffer();
	D3D12_INDEX_BUFFER_VIEW GetIndexBuffer();
	unsigned int GetIndexCount();
	std::vector<XMFLOAT3> GetPoints();

	//load fbx files
	void LoadFBX(ComPtr<ID3D12Device> device , std::string& filename);
	//method to load obj files
	void LoadOBJ(ComPtr<ID3D12Device> device , std::string& fileName, ComPtr<ID3D12GraphicsCommandList> commandList);

	//function to load draw the mesh
	//void Draw(ID3D11DeviceContext* context);
};

