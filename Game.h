#pragma once
#include "DXCore.h"
#include "Camera.h"
#include"Mesh.h"
#include"Entity.h"
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
		XMFLOAT4X4 view;
		XMFLOAT4X4 projection;
		XMFLOAT4X4 world;
	};

	int sceneConstantBufferAlignmentSize;

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

	ComPtr<ID3D12Resource> depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;

	std::shared_ptr<Camera> mainCamera;

	std::shared_ptr<Mesh> mesh1;
	std::shared_ptr<Entity> entity1;
	std::shared_ptr<Mesh> mesh2;


	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;
};

