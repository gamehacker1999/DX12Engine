#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
using namespace DirectX;

//class to represent the a movable camera
class Camera
{
protected:
	//fields for the class
	//view and projection matrices
	XMFLOAT4X4 viewMatrix;
	XMFLOAT4X4 projectionMatrix;

	//vectors to describe position and direction
	XMFLOAT3 position;
	XMFLOAT3 direction;
	XMFLOAT3 up;

	//roation angles around x and y axis
	float xRotation;
	float yRotation;

public:
	Camera(XMFLOAT3 position, XMFLOAT3 direction, XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f));
	virtual ~Camera();

	//getters and setters
	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetProjectionMatrix();
	XMFLOAT4X4 GetInverseProjection();

	//method to create projection matrix
	void CreateProjectionMatrix(float aspectRatio);

	//create view matrix
	void SetPositionTargetAndUp(XMFLOAT3 position, XMFLOAT3 direction, XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f));

	//function to get keyboard input
	void ManageKeyboard(float deltaTime);

	//changing the x and y of the mouse to rotate the camera
	void ChangeYawAndPitch(float deltaX, float deltaY);

	//getters
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetDirection();
	void SetPosition(XMFLOAT3 pos);
	void InvertPitch();

	//method to update the camera
	virtual void Update(float deltaTime);
};