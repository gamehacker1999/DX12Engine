#include "Camera.h"

Camera::Camera(Vector3 position, Vector3 direction, Vector3 up)
{
	xRotation = 0.0f;
	yRotation = 0.0f;

	//storing the position, direction, and up
	this->direction = direction;
	this->position = position;
	this->up = up;

	//calculating the view matrix of the camera
	auto tempView = XMMatrixLookToLH(XMLoadFloat3(&this->position),
		XMLoadFloat3(&this->direction), XMLoadFloat3(&this->up));

	//storing this value in view matrix
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(tempView));

}

Camera::~Camera()
{
}

Matrix& Camera::GetViewMatrix()
{
	//returning view matrix
		//creating a camera rotation matrix based on the x and y values
	XMVECTOR cameraRot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(yRotation), XMConvertToRadians(xRotation), 0.0f);

	//rotating the direction vector
	Vector3 unitZ(0.0f, 0.0f, 1.0f);
	XMVECTOR tempRotation = XMVector3Rotate(XMLoadFloat3(&unitZ), cameraRot);

	//storing the direction vector
	XMStoreFloat3(&direction, tempRotation);

	Vector3 worldUp(0.0f, 1.0f, 0.0f);
	XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
	XMStoreFloat3(&up, XMVector3Cross(XMLoadFloat3(&direction), right)); //finding the up vector

	//calculating the view matrix
	//calculating the view matrix of the camera
	auto tempView = XMMatrixLookToLH(XMLoadFloat3(&this->position),
		XMLoadFloat3(&this->direction), XMLoadFloat3(&this->up));

	//storing this value in view matrix
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(tempView));

	return viewMatrix;
}

Matrix& Camera::GetProjectionMatrix()
{
	return projectionMatrix;
}

Matrix& Camera::GetInverseProjection()
{
	XMMATRIX inverseProjTemp = XMLoadFloat4x4(&projectionMatrix);
	inverseProjTemp = XMMatrixInverse(nullptr, inverseProjTemp);

	Matrix invP;
	XMStoreFloat4x4(&invP, inverseProjTemp);

	return invP;
}

Matrix& Camera::GetInverseView()
{
	XMMATRIX inverseViewTemp = XMLoadFloat4x4(&viewMatrix);
	inverseViewTemp = XMMatrixInverse(nullptr, inverseViewTemp);

	Matrix invV;
	XMStoreFloat4x4(&invV, inverseViewTemp);

	return invV;
}

void Camera::CreateProjectionMatrix(float aspectRatio)
{
	//creating the projection matrix
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * 3.1415926535f,		// Field of View Angle
		aspectRatio,				// Aspect ratio
		0.1f,						// Near clip plane distance
		2000.0f);					// Far clip plane distance
	//XMMATRIX P = XMMatrixOrthographicLH(16, 9, 0.1f, 1000.0f);

	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!

}

void Camera::SetPositionTargetAndUp(Vector3 position, Vector3 direction, Vector3 up)
{
	xRotation = 0.0f;
	yRotation = 0.0f;

	//storing the position, direction, and up
	this->direction = direction;
	this->position = position;
	this->up = up;

	//calculating the view matrix of the camera
	auto tempView = XMMatrixLookToLH(XMLoadFloat3(&this->position),
		XMLoadFloat3(&this->direction), XMLoadFloat3(&this->up));

	//storing this value in view matrix
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(tempView));
}

void Camera::ManageKeyboard(float deltaTime, std::unique_ptr<Mouse>& mouse, std::unique_ptr<Keyboard>& keyboard)
{	

	auto state = mouse->GetState();
	auto kb = keyboard->GetState();
	//move back
	if (state.rightButton) 
	{
		if (kb.W)
		{
			position = position + direction * deltaTime * 10;//moving the camera forward
		}

		if (kb.S)
		{
			position = position - direction * deltaTime * 10;//moving the camera forward
		}

		//move right
		if (kb.D)
		{
			Vector3 worldUp(0.0f, 1.0f, 0.0f);
			auto right = worldUp.Cross(direction);
			position = position + right * deltaTime * 5;//moving the camera forward
		}

		//move left
		if (kb.A)
		{
			Vector3 worldUp(0.0f, 1.0f, 0.0f);
			auto right = worldUp.Cross(direction);
			position = position - right * deltaTime * 5;//moving the camera forward
		}

		//move down
		if (kb.Q)
		{
			Vector3 worldUp(0.0f, 1.0f, 0.0f);
			auto right = worldUp.Cross(direction);//finding the right vector
			up = direction.Cross(right); 
		    position = position - worldUp * deltaTime;//moving the camera forward
		}

		//move up
		if (kb.E)
		{
			Vector3 worldUp(0.0f, 1.0f, 0.0f);
			auto right = worldUp.Cross(direction);//finding the right vector
			up = direction.Cross(right);
			position = position + worldUp * deltaTime;//moving the camera forward
		}
	}
}

void Camera::ChangeYawAndPitch(float deltaX, float deltaY)
{
	//changing the x and y rotation values
	xRotation += deltaX * 0.05f;
	yRotation += deltaY * 0.05f;

	if (yRotation > 85.0f)
		yRotation = 85.0f;

	if (yRotation < -85.0f)
		yRotation = -85.0f;
}

Vector3& Camera::GetPosition()
{
	return position;
}

Vector3& Camera::GetDirection()
{
	return direction;
}

void Camera::SetPosition(Vector3 pos)
{
	position = pos;
}

void Camera::InvertPitch()
{
	yRotation *= -1;
}

void Camera::JitterProjMatrix(float x, float y)
{
	projectionMatrix._13 = x;
	projectionMatrix._23 = y;
}

void Camera::GetRayOriginAndDirection(int xPos, int yPos, float width, float height, Vector4& origin, Vector4& direction)
{
	auto proj = GetProjectionMatrix();
	proj.Transpose();

	float vX = (2.0f * xPos / width - 1.0f) / proj(0, 0);
	float vY = (-2.0f * yPos / height + 1) / proj(1, 1);

	origin = Vector4(0.f, 0.f, 0.f, 1.0f);
	direction = Vector4(vX, vY, 1.0f, 0.0f);
}


void Camera::Update(float deltaTime, std::unique_ptr<Mouse>& mouse, std::unique_ptr<Keyboard>& keyboard)
{

	//managing keyboard input
	ManageKeyboard(deltaTime, mouse, keyboard);

	//creating a camera rotation matrix based on the x and y values
	Quaternion cameraRot = Quaternion::CreateFromYawPitchRoll(XMConvertToRadians(xRotation), XMConvertToRadians(yRotation), 0.0f);

	//rotating the direction vector
	Vector3 unitZ(0.0f, 0.0f, 1.0f);
	//XMVECTOR tempRotation = XMVector3Rotate(XMLoadFloat3(&unitZ), cameraRot);
	direction = Vector3::Transform(unitZ, cameraRot);

	Vector3 worldUp(0.0f, 1.0f, 0.0f);
	auto right = worldUp.Cross(direction);//XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
	up = direction.Cross(right);

	//calculating the view matrix
	//calculating the view matrix of the camera
	//auto tempView = XMMatrixLookToLH(XMLoadFloat3(&this->position),
	//	XMLoadFloat3(&this->direction), XMLoadFloat3(&this->up));
	//
	////storing this value in view matrix
	//XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(tempView));

	viewMatrix = Matrix::CreateLookAt(position, position + direction, up);

	viewMatrix.Transpose();
}