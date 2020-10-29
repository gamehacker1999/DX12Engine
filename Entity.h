#pragma once
#include"Mesh.h"
#include"DX12Helper.h"
#include"Vertex.h"
#include<string>
#include"DescriptorHeapWrapper.h"
#include"Lights.h"
#include"Material.h"
#include<memory>
#include<entity\registry.hpp>
#include"Velocity.h"
//#include "RigidBody.h"
//#include"Material.h"

struct SceneConstantBuffer
{
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT4X4 world;
	XMFLOAT4X4 worldInvTranspose;
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

	D3DX12Residency::ManagedObject managedCBV;

	std::string tag;

	//ComPtr<ID3D12Resource> sceneConstantBufferResource;
	ManagedResource sceneConstantBufferResource;
	UINT8* constantBufferBegin;
	SceneConstantBuffer constantBufferData;
	bool isAlive;

	//std::shared_ptr<RigidBody> body;
	bool useRigidBody;

	//entity id for the ECS
	entt::entity entityID;

	//descriptor heap
	DescriptorHeapWrapper descriptorHeap;

public:
	//constructor which accepts a mesh
	Entity(std::shared_ptr<Mesh> mesh/**/, std::shared_ptr<Material>& material, entt::registry& registry);
	virtual ~Entity();

	//getters and setters
	void SetPosition(XMFLOAT3 position);
	void SetRotation(XMFLOAT4 rotation);
	void SetOriginalRotation(XMFLOAT4 rotation);
	void SetScale(XMFLOAT3 scale);
	void SetModelMatrix(XMFLOAT4X4 matrix);
	void Draw(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList, std::shared_ptr<GPUHeapRingBuffer> ringBuffer);
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
	XMMATRIX GetRawModelMatrix();

	entt::entity GetEntityID();

	void SetTag(std::string tag);
	std::string GetTag();

	void Die();

	bool GetAliveState();

	std::shared_ptr<Mesh> GetMesh();
	UINT GetMaterialIndex();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12RootSignature>& GetRootSignature();

	DescriptorHeapWrapper& GetDescriptorHeap();

	//std::shared_ptr<Material> GetMaterial();

	//method that prepares the material and sends it to the gpu
	void PrepareConstantBuffers(ComPtr<ID3D12Device> device,D3DX12Residency::ResidencyManager resManager,
		std::shared_ptr<D3DX12Residency::ResidencySet>& residencySet);
	void PrepareMaterial(XMFLOAT4X4 view, XMFLOAT4X4 projection);

	virtual void Update(float deltaTime);
	virtual void GetInput(float deltaTime);

	virtual bool IsColliding(std::shared_ptr<Entity> other);
};