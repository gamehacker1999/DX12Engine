#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> mesh/*, std::shared_ptr<Material> material*/)
{
	this->mesh = mesh;
	//this->material = material;

	XMStoreFloat4x4(&modelMatrix, XMMatrixIdentity()); //setting model matrix as identity

	//initializing position scale and rotation
	position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	XMStoreFloat4(&rotation, XMQuaternionIdentity()); //identity quaternion

	//body = nullptr;

	//don't need to recalculate matrix now
	recalculateMatrix = false;

	tag = "default";

	isAlive = true;

	useRigidBody = false;
}

Entity::~Entity()
{
}

void Entity::SetPosition(XMFLOAT3 position)
{
	this->position = position;
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetRotation(XMFLOAT4 rotation)
{
	this->rotation = rotation;
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetOriginalRotation(XMFLOAT4 rotation)
{
}

void Entity::SetScale(XMFLOAT3 scale)
{
	this->scale = scale;
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetModelMatrix(XMFLOAT4X4 matrix)
{
	this->modelMatrix = matrix;
}

/*void Entity::SetRigidBody(std::shared_ptr<RigidBody> body)
{
	this->body = body;
	useRigidBody = true;
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	this->body->SetModelMatrix(transpose);
}*/

/*std::shared_ptr<RigidBody> Entity::GetRigidBody()
{
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	body->SetModelMatrix(transpose);
	return body;
}*/

/*void Entity::UseRigidBody()
{
	body = std::make_shared<RigidBody>(mesh->GetPoints());
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	body->SetModelMatrix(transpose);
	useRigidBody = true;
}*/

XMFLOAT3 Entity::GetPosition()
{
	return position;
}

XMFLOAT3 Entity::GetForward()
{
	XMFLOAT3 forward;
	XMStoreFloat3(&forward, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&rotation)));
	return forward;
}

XMFLOAT3 Entity::GetRight()
{
	XMFLOAT3 right;
	XMStoreFloat3(&right, XMVector3Rotate(XMVectorSet(1, 0, 0, 0), XMLoadFloat4(&rotation)));
	return right;
}

XMFLOAT3 Entity::GetUp()
{
	XMFLOAT3 up;
	XMStoreFloat3(&up, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), XMLoadFloat4(&rotation)));
	return up;
}

XMFLOAT3 Entity::GetScale()
{
	return scale;
}

XMFLOAT4 Entity::GetRotation()
{
	return rotation;
}

XMFLOAT4X4 Entity::GetModelMatrix()
{
	//check if matrix has to be recalculated
	//if yes then calculate it 
	if (recalculateMatrix)
	{
		//getting the translation, scale, and rotation matrices
		XMMATRIX translate = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
		XMMATRIX scaleMat = XMMatrixScalingFromVector(XMLoadFloat3(&scale));
		XMMATRIX rotationMat = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));

		//calculating the model matrix from these three matrices and storing it
		//we transpose it before storing the matrix
		XMStoreFloat4x4(&modelMatrix, XMMatrixTranspose(scaleMat * rotationMat * translate));

		recalculateMatrix = false;
	}

	//returning the model matrix
	return modelMatrix;
}

void Entity::SetTag(std::string tag)
{
	this->tag = tag;
}

std::string Entity::GetTag()
{
	return tag;
}

void Entity::Die()
{
	isAlive = false;
}

bool Entity::GetAliveState()
{
	return isAlive;
}

std::shared_ptr<Mesh> Entity::GetMesh()
{
	return mesh;
}

/*std::shared_ptr<Material> Entity::GetMaterial()
{
	return material;
}*/

/*void Entity::PrepareMaterial(XMFLOAT4X4 view, XMFLOAT4X4 projection)
{
	//setting the appropriate data for the shader
	material->GetVertexShader()->SetMatrix4x4("world", GetModelMatrix());
	material->GetVertexShader()->SetMatrix4x4("view", view);
	material->GetVertexShader()->SetMatrix4x4("projection", projection);

	//setting the shaders as active
	material->GetVertexShader()->SetShader();
	material->GetPixelShader()->SetShader();

	//copying data to gpu
	material->GetVertexShader()->CopyAllBufferData();
	//material->GetPixelShader()->CopyAllBufferData();
}*/

void Entity::Update(float deltaTime)
{

}

void Entity::GetInput(float deltaTime)
{
}

bool Entity::IsColliding(std::shared_ptr<Entity> other)
{
	return false;
}