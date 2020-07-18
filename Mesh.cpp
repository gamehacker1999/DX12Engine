#include "Mesh.h"
Mesh::Mesh(Vertex* vertices, unsigned int numVertices, unsigned int* indices, int numIndices, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue)
{
	ComPtr<ID3D12Resource> vertexBufferDeafult;
	vertexBuffer = CreateVBView(vertices, numVertices, device, commandList, defaultHeap,uploadHeap);
	indexBuffer = CreateIBView(indices, numIndices, device, commandList, defaultIndexHeap, uploadIndexHeap);

	//WaitForPreviousFrame();
	//game->WaitForPreviousFrame();
	this->numIndices = numIndices;
}

Mesh::Mesh(std::string fileName, ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue)
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

	else if (fileName.find(".sdkmesh") != std::string::npos && commandQueue)
	{
		LoadSDKMesh(device, fileName, commandList, commandQueue);
	}

}

void Mesh::CalculateTangents(std::vector<Vertex>& vertices, std::vector<XMFLOAT3>& position, std::vector<XMFLOAT2>& uvs, unsigned int vertCount)
{
	//compute the tangents and bitangents for each triangle
	for (size_t i = 0; i < vertCount; i += 3)
	{
		//getting the position, normal, and uv data for vertex
		XMFLOAT3 vert1 = vertices[i].Position;
		XMFLOAT3 vert2 = vertices[i + 1].Position;
		XMFLOAT3 vert3 = vertices[i + 2].Position;

		XMFLOAT2 uv1 = vertices[i].UV;
		XMFLOAT2 uv2 = vertices[i + 1].UV;
		XMFLOAT2 uv3 = vertices[i + 2].UV;

		//finding the two edges of the triangles
		auto tempEdge = XMLoadFloat3(&vert2) - XMLoadFloat3(&vert1);
		XMFLOAT3 edge1;
		XMStoreFloat3(&edge1, tempEdge);
		tempEdge = XMLoadFloat3(&vert3) - XMLoadFloat3(&vert1);
		XMFLOAT3 edge2;
		XMStoreFloat3(&edge2, tempEdge);

		//finding the difference in UVs
		XMFLOAT2 deltaUV1;
		XMStoreFloat2(&deltaUV1, XMLoadFloat2(&uv2) - XMLoadFloat2(&uv1));
		XMFLOAT2 deltaUV2;
		XMStoreFloat2(&deltaUV2, XMLoadFloat2(&uv3) - XMLoadFloat2(&uv1));

		//calculate the inverse of the delta uv matrix
		float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
		//calculating the tangent of the triangle
		XMFLOAT3 tangent;
		XMStoreFloat3(&tangent, (XMLoadFloat3(&edge1) * deltaUV2.y - XMLoadFloat3(&edge2) * deltaUV1.y) * r);

		vertices[i].Tangent = tangent;
		vertices[i + 1].Tangent = tangent;
		vertices[i + 2].Tangent = tangent;

	}
}

Mesh::~Mesh()
{
}

std::pair<ComPtr<ID3D12Resource>, UINT> Mesh::GetVertexBufferResourceAndCount()
{
	return std::pair<ComPtr<ID3D12Resource>, UINT>(defaultHeap,static_cast<UINT>(vertices.size()));
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBuffer()
{
	return vertexBuffer;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBuffer()
{
	return indexBuffer;
}

ComPtr<ID3D12Resource> Mesh::GetVertexBufferResource()
{
	return defaultHeap;
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

	bool binExists = false;

	std::string binaryFileName = fileName + ".bin";

	std::ifstream fileStream;
	fileStream.open(binaryFileName.c_str(), std::ios::in | std::ios::binary);

	if (fileStream.fail())
	{
		binExists = false;
		fileStream.close();
	}

	else
	{
		UINT vertCount = 0;
		UINT indicesCount = 0;
		fileStream.read(reinterpret_cast<char*>(&vertCount), sizeof(vertCount));
		fileStream.read(reinterpret_cast<char*>(&indicesCount), sizeof(indicesCount));
		vertices.resize(vertCount);
		indices.resize(indicesCount, 0);
		fileStream.read(reinterpret_cast<char*>(&vertices[0]), sizeof(Vertex)*vertCount);
		fileStream.read(reinterpret_cast<char*>(&indices[0]), sizeof(UINT)* indicesCount);
		fileStream.close();
		vertexBuffer = CreateVBView(vertices.data(), vertCount, device, commandList, defaultHeap, uploadHeap);
		indexBuffer = CreateIBView(indices.data(), vertCount, device, commandList, defaultIndexHeap, uploadIndexHeap);
		this->numIndices = vertCount;
		return;
	}

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

		CalculateTangents(vertices, positions, uvs, vertCount);
		points = positions;

		this->numIndices = vertCount;

		vertexBuffer = CreateVBView(vertices.data(), vertCount, device, commandList, defaultHeap, uploadHeap);
		indexBuffer = CreateIBView(indices.data(), vertCount, device, commandList, defaultIndexHeap, uploadIndexHeap);

		if (!binExists)
		{
			std::ofstream fout((fileName + ".bin"), std::ios::out | std::ios::binary);
			UINT vertexCount = vertices.size();
			UINT indicesCount = indices.size();
			fout.write(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
			fout.write(reinterpret_cast<char*>(&indicesCount), sizeof(indicesCount));
			fout.write(reinterpret_cast<char*>(&vertices[0]), vertices.size() * sizeof(Vertex));
			fout.write(reinterpret_cast<char*>(&indices[0]), indices.size() * sizeof(UINT));

			fout.close();
		}

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

void Mesh::LoadSDKMesh(ComPtr<ID3D12Device> device, std::string& fileName, ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue)
{
	std::wstring name = std::wstring(fileName.begin(), fileName.end());
	auto model = Model::CreateFromSDKMESH(name.c_str(), device.Get());

	ResourceUploadBatch resourceUploadBatch(device.Get());

	resourceUploadBatch.Begin();
	model->LoadStaticBuffers(device.Get(), resourceUploadBatch);

	auto uploadResourcesFinished = resourceUploadBatch.End(commandQueue.Get());

	uploadResourcesFinished.wait();
}
