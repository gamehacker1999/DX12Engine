#pragma once
#include "DXCore.h"
#include "Camera.h"
#include <DirectXMath.h>

class Game
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	HRESULT Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void PopulateCommandList();
	void WaitForPreviousFrame();

	// Overridden mouse input helper methods
	void OnMouseDown(WPARAM buttonState, int x, int y);
	void OnMouseUp(WPARAM buttonState, int x, int y);
	void OnMouseMove(WPARAM buttonState, int x, int y);
	void OnMouseWheel(float wheelDelta, int x, int y);
private:

	struct SceneConstantBuffer
	{
		XMFLOAT4 offset;
	};


	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadShaders();
	void CreateMatrices();
	void CreateBasicGeometry();

	//app resources
	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// The matrices to go from model space to screen space
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;

	ComPtr<ID3D12DescriptorHeap> constantBufferHeap;
	ComPtr<ID3D12Resource> constantBuffer;
	SceneConstantBuffer constantBufferData;
	UINT8* constantBufferBegin;

	std::shared_ptr<Camera> mainCamera;

	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;
};

