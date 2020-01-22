#include "Mesh.h"
#include"Game.h"
Mesh::Mesh(Vertex* vertices, unsigned int numVertices, unsigned int* indices, int numIndices, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	ComPtr<ID3D12Resource> vertexBufferDeafult;
	vertexBuffer = CreateVBView(vertices, numVertices, device, commandList, defaultHeap,uploadHeap);
	indexBuffer = CreateIBView(indices, numIndices, device, commandList, defaultIndexHeap, uploadIndexHeap);

	//WaitForPreviousFrame();
	//game->WaitForPreviousFrame();
	this->numIndices = numIndices;
}

Mesh::Mesh(std::string fileName, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	vertexBuffer = {};
	indexBuffer = {};
	numIndices = 0;

	if (fileName.find(".fbx") != std::string::npos)
	{
		LoadFBX(device, fileName);
	}

	else if (fileName.find(".obj") != std::string::npos)
	{
		LoadOBJ(device, fileName,commandList);
	}

}

void Mesh::CalculateTangents(std::vector<Vertex>& vertices, std::vector<XMFLOAT3>& position, std::vector<XMFLOAT2>& uvs, unsigned int vertCount)
{
}

Mesh::~Mesh()
{
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBuffer()
{
	return vertexBuffer;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBuffer()
{
	return indexBuffer;
}

unsigned int Mesh::GetIndexCount()
{
	return numIndices;
}

std::vector<XMFLOAT3> Mesh::GetPoints()
{
	return std::vector<XMFLOAT3>();
}

void Mesh::LoadFBX(ComPtr<ID3D12Device> device, std::string& filename)
{
}

void Mesh::LoadOBJ(ComPtr<ID3D12Device> device, std::string& fileName, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	std::ifstream ifile(fileName.c_str());

	std::string line; //line that stores file data

	//check if the file exists
	if (ifile.is_open())
	{
		//making a list of the position, normals and uvs
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT3> normals;
		std::vector<XMFLOAT2> uvs;
		std::vector<XMFLOAT3> tangents;
		std::vector<XMFLOAT3> bitangents;

		//list of vertices and indices
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;


		//num of verts and indices
		UINT vertCount = 0;
		UINT indexCount = 0;

		while (std::getline(ifile, line))
		{
			std::vector<std::string> words; //this holds all the individual characters of the line

			size_t pos = 0;
			size_t curPos = 0;

			//splitting the string with the spacebar
			while (pos <= line.length())
			{
				//finding the space string
				//taking a substring from that point
				//storing that substring in the list
				pos = line.find(" ", curPos);
				std::string word = line.substr(curPos, (size_t)(pos - curPos));
				curPos = pos + 1;
				words.emplace_back(word);
			}

			if (line.find("v ") == 0)
			{
				//storing the position if the line starts with "v"
				positions.emplace_back(XMFLOAT3(std::stof(words[1]), std::stof(words[2]), std::stof(words[3])));
			}

			if (line.find("vn") == 0)
			{
				//storing the normals if the line starts with "vn"
				normals.emplace_back(XMFLOAT3(std::stof(words[1]), std::stof(words[2]), std::stof(words[3])));
			}

			if (line.find("vt") == 0)
			{
				//storing the textures if the line starts with "vt"
				uvs.emplace_back(XMFLOAT2(std::stof(words[1]), std::stof(words[2])));
			}

			if (line.find("f") == 0)
			{
				//storing the faces if the line starts with "f"

				//this the the list of the faces on this line
				std::vector<std::vector<std::string>> listOfFaces;

				listOfFaces.reserve(10);

				//looping through all the faces
				for (int i = 0; i < words.size() - 1; i++)
				{
					std::vector<std::string> face; //holds the individial data of each face
					face.reserve(10);
					pos = 0;
					curPos = 0;

					//splitting each vertex further to seperate it based on a '/' character
					while (pos <= words[(size_t)i + 1].length())
					{
						(pos = words[(size_t)i + 1].find("/", curPos));
						std::string word = words[(size_t)i + 1].substr(curPos, pos - curPos);
						curPos = pos + 1;
						face.emplace_back(word);
					}

					//adding this face to the list of faces
					listOfFaces.emplace_back(face);
				}

				//first vertex
				Vertex v1;
				v1.Position = positions[(size_t)std::stoi(listOfFaces[0][0]) - 1];
				v1.Normal = normals[(size_t)std::stoi(listOfFaces[0][2]) - 1];
				v1.UV = uvs[(size_t)std::stoi(listOfFaces[0][1]) - 1];

				//second vertex
				Vertex v2;
				v2.Position = positions[(size_t)std::stoi(listOfFaces[1][0]) - 1];
				v2.Normal = normals[(size_t)std::stoi(listOfFaces[1][2]) - 1];
				v2.UV = uvs[(size_t)std::stoi(listOfFaces[1][1]) - 1];

				//third vertex
				Vertex v3;
				v3.Position = positions[(size_t)std::stoi(listOfFaces[2][0]) - 1];
				v3.Normal = normals[(size_t)std::stoi(listOfFaces[2][2]) - 1];
				v3.UV = uvs[(size_t)std::stoi(listOfFaces[2][1]) - 1];

				//since the texture space starts at top left, it its probably upside down
				v1.UV.y = 1 - v1.UV.y;
				v2.UV.y = 1 - v2.UV.y;
				v3.UV.y = 1 - v3.UV.y;

				//since the coordinates system are inverted we have to flip the normals and 
				//negate the z axis of position
				v1.Normal.z *= -1;
				v2.Normal.z *= -1;
				v3.Normal.z *= -1;

				v1.Position.z *= -1;
				v2.Position.z *= -1;
				v3.Position.z *= -1;

				//we have to flip the winding order
				vertices.emplace_back(v1);
				vertices.emplace_back(v3);
				vertices.emplace_back(v2);

				//adding indices
				indices.emplace_back(vertCount); vertCount++;
				indices.emplace_back(vertCount); vertCount++;
				indices.emplace_back(vertCount); vertCount++;


				//if it has a potential 4th vertex, add it, but ignore n-gons
				if (listOfFaces.size() > 3)
				{
					//fourth vertex
					Vertex v4;
					v4.Position = positions[(size_t)std::stoi(listOfFaces[3][0]) - 1];
					v4.Normal = normals[(size_t)std::stoi(listOfFaces[3][2]) - 1];
					v4.UV = uvs[(size_t)std::stoi(listOfFaces[3][1]) - 1];

					//do the same handedness conversion
					v4.Position.z *= -1;
					v4.Normal.z *= -1;
					v4.UV.y = 1 - v4.UV.y;

					//adding the triangle
					vertices.emplace_back(v1);
					vertices.emplace_back(v4);
					vertices.emplace_back(v3);

					//adding indices
					indices.emplace_back(vertCount); vertCount++;
					indices.emplace_back(vertCount); vertCount++;
					indices.emplace_back(vertCount); vertCount++;

				}
			}


		}

		//CalculateTangents(vertices, positions, uvs, vertCount);
		points = positions;

		this->numIndices = vertCount;

		vertexBuffer = CreateVBView(vertices.data(), vertCount, device, commandList, defaultHeap, uploadHeap);
		indexBuffer = CreateIBView(indices.data(), vertCount, device, commandList, defaultIndexHeap, uploadIndexHeap);

		//create the vertex and index buffer
		//vertexBuffer
		/*D3D11_BUFFER_DESC vbd;
		memset(&vbd, 0, sizeof(vbd));
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		vbd.ByteWidth = vertCount * sizeof(Vertex);

		//creating a subresource
		D3D11_SUBRESOURCE_DATA initialVertexData;
		initialVertexData.pSysMem = reinterpret_cast<void*>(vertices.data()); //the vertex list

		device->CreateBuffer(&vbd, &initialVertexData, &vertexBuffer);

		//index buffer description
		D3D11_BUFFER_DESC ibd;
		memset(&ibd, 0, sizeof(ibd));
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = vertCount * sizeof(unsigned int);

		D3D11_SUBRESOURCE_DATA initialIndexData;
		initialIndexData.pSysMem = reinterpret_cast<void*>(indices.data());

		//creating index buffer
		device->CreateBuffer(&ibd, &initialIndexData, &indexBuffer);*/
		ifile.close();
	}
}
