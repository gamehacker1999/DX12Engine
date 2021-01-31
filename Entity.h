#pragma once
#include"Mesh.h"
#include "MyModel.h"
#include"DX12Helper.h"
#include"Vertex.h"
#include<string>
#include"DescriptorHeapWrapper.h"
#include"Lights.h"
#include"Material.h"
#include<memory>
#include<entity\registry.hpp>
#include"Velocity.h"
#include <DirectXCollision.h>
//#include "RigidBody.h"
//#include"Material.h"
struct SceneConstantBuffer
{
	Matrix view;
	Matrix projection;
	Matrix world;
	Matrix worldInvTranspose;
};


class Entity
{
protected:
	//vectors for scale and position
	Vector3 position;
	Vector3 scale;

	//quaternion for rotation
	Quaternion rotation;

	EulerAngles angles;

	//model matrix of the entity
	Matrix modelMatrix;

	//model matrix of the entity
	Matrix prevModelMatrix;

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

	//adding a model
	std::shared_ptr<MyModel> model;

	//descriptor heap
	DescriptorHeapWrapper descriptorHeap;

	//pipeline and root sig
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12RootSignature> rootSig;
	//bounding box
	DirectX::BoundingBox bounds;

public:
	//constructor which accepts a mesh
	Entity(entt::registry& registry, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig, std::string name ="default");

	virtual ~Entity();

	//getters and setters
	void SetPosition(Vector3 position);
	void SetRotation(Quaternion rotation);
	void SetRotation(float yaw, float pitch, float roll);
	void SetOriginalRotation(Vector4 rotation);
	void SetScale(Vector3 scale);
	void SetModelMatrix(Matrix matrix);
	void Draw(const ComPtr<ID3D12GraphicsCommandList>& commandList, std::shared_ptr<GPUHeapRingBuffer>& ringBuffer);
	//void SetRigidBody(std::shared_ptr<RigidBody> body);
	//std::shared_ptr<RigidBody> GetRigidBody();
	void UseRigidBody();

	Vector3& GetPosition();
	Vector3& GetForward();
	Vector3& GetRight();
	Vector3& GetUp();

	Vector3& GetScale();
	Quaternion& GetRotation();
	Matrix& GetModelMatrix();
	Matrix& GetPrevModelMatrix();
	XMMATRIX& GetRawModelMatrix();

	entt::entity& GetEntityID();

	void SetTag(std::string tag);
	std::string GetTag();

	void Die();

	bool GetAliveState();

	std::shared_ptr<Mesh>& GetMesh();
	UINT& GetMaterialIndex();
	ComPtr<ID3D12PipelineState>& GetPipelineState();
	ComPtr<ID3D12RootSignature>& GetRootSignature();

	DescriptorHeapWrapper& GetDescriptorHeap();

	void AddModel(std::string pathToFile);
	std::shared_ptr<MyModel>& GetModel();
	void AddMaterial(unsigned int matId);

	DirectX::BoundingBox GetBounds();

	//std::shared_ptr<Material> GetMaterial();

	void ManipulateTransforms(Matrix& view, Matrix& proj, ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE);

	//method that prepares the material and sends it to the gpu
	void PrepareConstantBuffers(D3DX12Residency::ResidencyManager resManager,
		std::shared_ptr<D3DX12Residency::ResidencySet>& residencySet);
	void PrepareMaterial(Matrix view, Matrix projection);

	virtual void Update(float deltaTime);
	virtual void GetInput(float deltaTime);

	void CreateBounds();

	bool RayBoundIntersection(Vector4& origin, Vector4& direction, float& dist, Matrix& viewMat);

	virtual bool IsColliding(std::shared_ptr<Entity> other);
};