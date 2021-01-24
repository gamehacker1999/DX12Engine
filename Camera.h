#pragma once
#include<d3d12.h>
#include "DX12Helper.h"
#include<DirectXMath.h>
#include<memory>
using namespace DirectX;

//class to represent the a movable camera
class Camera
{
protected:
	//fields for the class
	//view and projection matrices
	Matrix viewMatrix;
	Matrix projectionMatrix;

	//vectors to describe position and direction
	Vector3 position;
	Vector3 direction;
	Vector3 up;

	//roation angles around x and y axis
	float xRotation;
	float yRotation;

public:
	Camera(Vector3 position, Vector3 direction, Vector3 up = Vector3(0.0f, 1.0f, 0.0f));
	virtual ~Camera();

	//getters and setters
	Matrix GetViewMatrix();
	Matrix GetProjectionMatrix();
	Matrix GetInverseProjection();

	//method to create projection matrix
	void CreateProjectionMatrix(float aspectRatio);

	//create view matrix
	void SetPositionTargetAndUp(Vector3 position, Vector3 direction, Vector3 up = Vector3(0.0f, 1.0f, 0.0f));

	//function to get keyboard input
	void ManageKeyboard(float deltaTime, std::unique_ptr<Mouse>& mouse, std::unique_ptr<Keyboard>& keyboard);

	//changing the x and y of the mouse to rotate the camera
	void ChangeYawAndPitch(float deltaX, float deltaY);

	//getters
	Vector3 GetPosition();
	Vector3 GetDirection();
	void SetPosition(Vector3 pos);
	void InvertPitch();

	void GetRayOriginAndDirection(int xPos, int yPos, float width, float height, Vector4& origin, Vector4& direction);

	//method to update the camera
	virtual void Update(float deltaTime, std::unique_ptr<Mouse>& mouse, std::unique_ptr<Keyboard>& keyboard);
};