#include "Entity.h"

Entity::Entity(entt::registry& registry, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSig, std::string name)
{

	this->pipelineState = pipelineState;
	this->rootSig = rootSig;


	XMStoreFloat4x4(&modelMatrix, XMMatrixIdentity()); //setting model matrix as identity

	prevModelMatrix = modelMatrix;

	//initializing position scale and rotation
	position = Vector3(0.0f, 0.0f, 0.0f);
	scale = Vector3(1.0f, 1.0f, 1.0f);

	angles = {};

	XMStoreFloat4(&rotation, XMQuaternionIdentity()); //identity quaternion

	//body = nullptr;

	//don't need to recalculate matrix now
	recalculateMatrix = false;

	tag = name;

	isAlive = true;

	useRigidBody = false;

	entityID = registry.create();

	model = nullptr;
}

Entity::~Entity()
{
}

void Entity::SetPosition(Vector3 position)
{
	this->position = position;
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetRotation(Quaternion rotation)
{
	this->rotation = rotation;
	angles = ToEulerAngles(rotation);
	angles = { XMConvertToDegrees(angles.pitch), XMConvertToDegrees(angles.yaw), XMConvertToDegrees(angles.roll) };
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetRotation(float yaw, float pitch, float roll)
{
	rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
	angles = ToEulerAngles(rotation);
	angles = { XMConvertToDegrees(angles.pitch), XMConvertToDegrees(angles.yaw), XMConvertToDegrees(angles.roll) };
	recalculateMatrix = true;
}

void Entity::SetOriginalRotation(Vector4 rotation)
{
}

void Entity::SetScale(Vector3 scale)
{
	this->scale = scale;
	recalculateMatrix = true; //need to recalculate model matrix now
}

void Entity::SetModelMatrix(Matrix matrix)
{
	this->modelMatrix = matrix;
}

void Entity::Draw(const ComPtr<ID3D12GraphicsCommandList>& commandList, std::shared_ptr<GPUHeapRingBuffer>& ringBuffer)
{
    commandList->SetGraphicsRootSignature(GetRootSignature().Get());

	ringBuffer->AddDescriptor(1, GetDescriptorHeap(), 0);

	commandList->SetGraphicsRootDescriptorTable(EntityRootIndices::EntityVertexCBV, ringBuffer->GetDynamicResourceOffset());

	if (model != nullptr)
	{
		model->Draw(commandList);
	}

}

/*void Entity::SetRigidBody(std::shared_ptr<RigidBody> body)
{
	this->body = body;
	useRigidBody = true;
	Matrix transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	this->body->SetModelMatrix(transpose);
}*/

/*std::shared_ptr<RigidBody> Entity::GetRigidBody()
{
	Matrix transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	body->SetModelMatrix(transpose);
	return body;
}*/

/*void Entity::UseRigidBody()
{
	body = std::make_shared<RigidBody>(mesh->GetPoints());
	Matrix transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(XMLoadFloat4x4(&GetModelMatrix())));
	body->SetModelMatrix(transpose);
	useRigidBody = true;
}*/

Vector3& Entity::GetPosition()
{
	return position;
}

Vector3& Entity::GetForward()
{
	Vector3 forward;
	XMStoreFloat3(&forward, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&rotation)));
	return forward;
}

Vector3& Entity::GetRight()
{
	Vector3 right;
	XMStoreFloat3(&right, XMVector3Rotate(XMVectorSet(1, 0, 0, 0), XMLoadFloat4(&rotation)));
	return right;
}

Vector3& Entity::GetUp()
{
	Vector3 up;
	XMStoreFloat3(&up, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), XMLoadFloat4(&rotation)));
	return up;
}

Vector3& Entity::GetScale()
{
	return scale;
}

Quaternion& Entity::GetRotation()
{
	return rotation;
}

Matrix& Entity::GetModelMatrix()
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

Matrix& Entity::GetPrevModelMatrix()
{
	return prevModelMatrix;
}

XMMATRIX& Entity::GetRawModelMatrix()
{
	//getting the translation, scale, and rotation matrices
	XMMATRIX translate = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
	XMMATRIX scaleMat = XMMatrixScalingFromVector(XMLoadFloat3(&scale));
	XMMATRIX rotationMat = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));

	auto mat = scaleMat * rotationMat * translate;
	return mat;
}

entt::entity& Entity::GetEntityID()
{
	return entityID;
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

std::shared_ptr<Mesh>& Entity::GetMesh()
{
	return mesh;
}

UINT& Entity::GetMaterialIndex()
{
	return this->material->materialIndex;
}

ComPtr<ID3D12PipelineState>& Entity::GetPipelineState()
{
	return pipelineState;
}

ComPtr<ID3D12RootSignature>& Entity::GetRootSignature()
{
	return rootSig;
}

DescriptorHeapWrapper& Entity::GetDescriptorHeap()
{
	return descriptorHeap;
}

void Entity::AddModel(std::string pathToFile)
{
	model = std::make_shared<MyModel>(pathToFile);

	CreateBounds();
}

std::shared_ptr<MyModel>& Entity::GetModel()
{
	if (model != nullptr)
	{
		return model;
	}

	std::shared_ptr<MyModel> temp = nullptr;
	return temp;
}

void Entity::AddMaterial(unsigned int matId)
{
	if (model)
	{
		model->SetMaterial(matId);
	}
}

DirectX::BoundingBox Entity::GetBounds()
{
	return bounds;
}

/*std::shared_ptr<Material> Entity::GetMaterial()
{
	return material;
}*/

/**/void Entity::PrepareMaterial(Matrix view, Matrix projection)
{

	constantBufferData.world = GetModelMatrix();
	constantBufferData.view = view;
	constantBufferData.projection = projection;

	XMMATRIX invTransposeTemp = GetRawModelMatrix();

	invTransposeTemp = XMMatrixInverse(nullptr, invTransposeTemp);
	XMStoreFloat4x4(&constantBufferData.worldInvTranspose, invTransposeTemp);

	memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
}


void Entity::ManipulateTransforms(Matrix& view, Matrix& proj, ImGuizmo::OPERATION op)
{

	float tr[3];
	float rot[3];
	float scl[3];

	auto objmat = GetModelMatrix();
	auto tempObjMat = XMLoadFloat4x4(&objmat);
	tempObjMat = XMMatrixTranspose(objmat);
	XMStoreFloat4x4(&objmat, tempObjMat);

	auto tempViewMat = XMLoadFloat4x4(&view);
	tempViewMat = XMMatrixTranspose(tempViewMat);
	XMStoreFloat4x4(&view, tempViewMat);

	auto tempProjMat = XMLoadFloat4x4(&proj);
	tempProjMat = XMMatrixTranspose(tempProjMat);
	XMStoreFloat4x4(&proj, tempProjMat);

	Vector3 tempScale;
	Vector3 tempTrans;
	Quaternion tempRot;

	objmat.Decompose(tempScale, tempRot, tempTrans);

	ImGuizmo::SetOrthographic(true);
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	Matrix deltaMatrix;
	ImGuizmo::Manipulate(*view.m, *proj.m, op, ImGuizmo::WORLD, *objmat.m, *deltaMatrix.m);

	objmat.Decompose(tempScale, tempRot, tempTrans);


	SetPosition(tempTrans);
	SetScale(tempScale);
	SetRotation(tempRot);
}

void Entity::PrepareConstantBuffers(D3DX12Residency::ResidencyManager resManager,
	std::shared_ptr<D3DX12Residency::ResidencySet>& residencySet)
{


	ThrowIfFailed(descriptorHeap.Create(1, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	descriptorHeap.CreateDescriptor(sceneConstantBufferResource, RESOURCE_TYPE_CBV, sizeof(SceneConstantBuffer));

	managedCBV.Initialize(sceneConstantBufferResource.resource.Get(), sizeof(SceneConstantBuffer));

	resManager.BeginTrackingObject(&managedCBV);
	residencySet->Insert(&managedCBV);

	CD3DX12_RANGE range(0, 0);

	ZeroMemory(&constantBufferData, sizeof(constantBufferData));
	ThrowIfFailed(sceneConstantBufferResource.resource->Map(0, &range, reinterpret_cast<void**>(&constantBufferBegin)));
	memcpy(constantBufferBegin, &constantBufferData, sizeof(constantBufferData));
}

void Entity::Update(float deltaTime)
{
	prevModelMatrix = modelMatrix;
}

void Entity::GetInput(float deltaTime)
{
}

void Entity::CreateBounds()
{
	auto meshes = model->GetMeshes();

	Vector3 minf3(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 maxf3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	auto vMin = XMLoadFloat3(&minf3);
	auto vMax = XMLoadFloat3(&maxf3);

	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto vertices = meshes[i]->GetVerts();

		for (size_t j = 0; j < vertices.size(); j++)
		{
			auto pos = XMLoadFloat3(&vertices[j].Position);

			vMin = XMVectorMin(vMin, pos);
			vMax = XMVectorMax(vMax, pos);

		}

	}

	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
}

bool Entity::RayBoundIntersection(Vector4& origin, Vector4& direction, float& dist, Matrix& viewMat)
{
	auto rayOrigin = XMLoadFloat4(&origin);
	auto rayDir = XMLoadFloat4(&direction);

	auto tempView = XMLoadFloat4x4(&viewMat);
	auto modelMat = GetModelMatrix();
	auto tempModel = XMLoadFloat4x4(&modelMat);

	tempView = XMMatrixTranspose(tempView);
	tempModel = XMMatrixTranspose(tempModel);
	auto invView = XMMatrixInverse(NULL, tempView);
	auto invModel = XMMatrixInverse(NULL, tempModel);

	rayOrigin = XMVector3TransformCoord(rayOrigin, invView);
	rayOrigin = XMVector3TransformCoord(rayOrigin, invModel);

	rayDir = XMVector3TransformNormal(rayDir, invView);
	rayDir = XMVector3TransformNormal(rayDir, invModel);

	rayDir = XMVector3Normalize(rayDir);

	if (!bounds.Intersects(rayOrigin, rayDir, dist))
	{
		return false;
	}

	auto meshes = model->GetMeshes();

	for (size_t i = 0; i < meshes.size(); i++)
	{
		if (meshes[i]->RayMeshTest(rayOrigin, rayDir))
		{
			return true;
		}
	}

	return false;
}

bool Entity::IsColliding(std::shared_ptr<Entity> other)
{
	return false;
}