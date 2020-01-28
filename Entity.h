#pragma once
#include"Mesh.h"
#include"DX12Helper.h"
#include"Vertex.h"
#include<string>
#include"Lights.h"
#include"Material.h"
#include<memory>
//#include "RigidBody.h"
//#include"Material.h"

struct SceneConstantBuffer
{
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT4X4 world;
	XMFLOAT4X4 padding;
};


class Entity
{
protected:
	//vectors for scale and position
	XMFLOAT3 position;
	XMFLOAT3 scale;

	//quaternion for rotation
	XMFLOAT4 rotation;

	//model matrix of the entity
	XMFLOAT4X4 modelMatrix;

	bool recalculateMatrix; // boolean to check if any transform has changed

	std::shared_ptr<Mesh> mesh; //mesh associated with this entity

	std::shared_ptr<Material> material; //material of this entity

	std::string tag;

	ComPtr<ID3D12Resource> sceneConstantBufferResource;
	UINT8* constantBufferBegin;
	SceneConstantBuffer constantBufferData;
	bool isAlive;

	//std::shared_ptr<RigidBody> body;
	bool useRigidBody;

public:
	//constructor which accepts a mesh
	Entity(std::shared_ptr<Mesh> mesh/**/, std::shared_ptr<Material>& material);
	virtual ~Entity();

	//getters and setters
	void SetPosition(XMFLOAT3 position);
	void SetRotation(XMFLOAT4 rotation);
	void SetOriginalRotation(XMFLOAT4 rotation);
	void SetScale(XMFLOAT3 scale);
	void SetModelMatrix(XMFLOAT4X4 matrix);
	//void SetRigidBody(std::shared_ptr<RigidBody> body);
	//std::shared_ptr<RigidBody> GetRigidBody();
	void UseRigidBody();

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetForward();
	XMFLOAT3 GetRight();
	XMFLOAT3 GetUp();

	XMFLOAT3 GetScale();
	XMFLOAT4 GetRotation();
	XMFLOAT4X4 GetModelMatrix();

	void SetTag(std::string tag);
	std::string GetTag();

	void Die();

	bool GetAliveState();

	std::shared_ptr<Mesh> GetMesh();

	//std::shared_ptr<Material> GetMaterial();

	//method that prepares the material and sends it to the gpu
	void PrepareConstantBuffers(CD3DX12_CPU_DESCRIPTOR_HANDLE& mainDescriptorHandle,
		ComPtr<ID3D12Device>& device);
	void PrepareMaterial(XMFLOAT4X4 view, XMFLOAT4X4 projection);

	virtual void Update(float deltaTime);
	virtual void GetInput(float deltaTime);

	virtual bool IsColliding(std::shared_ptr<Entity> other);
};