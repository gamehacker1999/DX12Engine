#include "Camera.h"

Camera::Camera(XMFLOAT3 position, XMFLOAT3 direction, XMFLOAT3 up)
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

XMFLOAT4X4 Camera::GetViewMatrix()
{
	//returning view matrix
		//creating a camera rotation matrix based on the x and y values
	XMVECTOR cameraRot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(yRotation), XMConvertToRadians(xRotation), 0.0f);

	//rotating the direction vector
	XMFLOAT3 unitZ(0.0f, 0.0f, 1.0f);
	XMVECTOR tempRotation = XMVector3Rotate(XMLoadFloat3(&unitZ), cameraRot);

	//storing the direction vector
	XMStoreFloat3(&direction, tempRotation);

	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
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

XMFLOAT4X4 Camera::GetProjectionMatrix()
{
	return projectionMatrix;
}

XMFLOAT4X4 Camera::GetInverseProjection()
{
	XMMATRIX inverseProjTemp = XMLoadFloat4x4(&projectionMatrix);
	inverseProjTemp = XMMatrixInverse(nullptr, inverseProjTemp);

	XMFLOAT4X4 invP;
	XMStoreFloat4x4(&invP, inverseProjTemp);

	return invP;
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

void Camera::SetPositionTargetAndUp(XMFLOAT3 position, XMFLOAT3 direction, XMFLOAT3 up)
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

void Camera::ManageKeyboard(float deltaTime)
{	
	//move back

	if (GetAsyncKeyState('W') & 0x8000)
	{
		XMVECTOR tempPosition = XMLoadFloat3(&position) + XMLoadFloat3(&direction) * deltaTime * 10;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position	
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		XMVECTOR tempPosition = XMLoadFloat3(&position) - XMLoadFloat3(&direction) * deltaTime * 10;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position	
	}

	//move right
	if (GetAsyncKeyState('D') & 0x8000)
	{
		XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
		XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
		XMVECTOR tempPosition = XMLoadFloat3(&position) + right * deltaTime * 5;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position	
	}

	//move left
	if (GetAsyncKeyState('A') & 0x8000)
	{
		XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
		XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
		XMVECTOR tempPosition = XMLoadFloat3(&position) - right * deltaTime * 5;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position
	}

	//move down
	if (GetAsyncKeyState('Q') & 0x8000)
	{
		XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
		XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
		XMStoreFloat3(&up, XMVector3Cross(XMLoadFloat3(&direction), right));
		XMVECTOR tempPosition = XMLoadFloat3(&position) - XMLoadFloat3(&worldUp) * deltaTime;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position
	}

	//move up
	if (GetAsyncKeyState('E') & 0x8000)
	{
		XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
		XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
		XMStoreFloat3(&up, XMVector3Cross(XMLoadFloat3(&direction), right));
		XMVECTOR tempPosition = XMLoadFloat3(&position) + XMLoadFloat3(&worldUp) * deltaTime;//moving the camera forward
		XMStoreFloat3(&position, tempPosition);// storing the position
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

XMFLOAT3 Camera::GetPosition()
{
	return position;
}

XMFLOAT3 Camera::GetDirection()
{
	return direction;
}

void Camera::SetPosition(XMFLOAT3 pos)
{
	position = pos;
}

void Camera::InvertPitch()
{
	yRotation *= -1;
}

void Camera::Update(float deltaTime)
{

	//managing keyboard input
	ManageKeyboard(deltaTime);

	//creating a camera rotation matrix based on the x and y values
	XMVECTOR cameraRot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(yRotation), XMConvertToRadians(xRotation), 0.0f);

	//rotating the direction vector
	XMFLOAT3 unitZ(0.0f, 0.0f, 1.0f);
	XMVECTOR tempRotation = XMVector3Rotate(XMLoadFloat3(&unitZ), cameraRot);

	//storing the direction vector
	XMStoreFloat3(&direction, tempRotation);

	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);
	XMVECTOR right = XMVector3Cross(XMLoadFloat3(&worldUp), XMLoadFloat3(&direction)); //finding the right vector
	XMStoreFloat3(&up, XMVector3Cross(XMLoadFloat3(&direction), right)); //finding the up vector

	//calculating the view matrix
	//calculating the view matrix of the camera
	auto tempView = XMMatrixLookToLH(XMLoadFloat3(&this->position),
		XMLoadFloat3(&this->direction), XMLoadFloat3(&this->up));

	//storing this value in view matrix
	XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(tempView));
}