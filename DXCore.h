#pragma once

#include <Windows.h>
#include <windowsx.h>
#include <d3d12.h>
#include<dxgi1_6.h>
#include<exception>
#include <string>
#include<sstream>
#include<memory>
#include<wrl/client.h>
#include"DX12Helper.h"
#include <dxcapi.h>
#include <vector>
#include"DescriptorHeapWrapper.h"
#include "TopLevelASGenerator.h"
#include "BottomLevelASGenerator.h"
#include "RaytracingPipelineGenerator.h"
#include "RootSignatureGenerator.h"
#include"ShaderBindingTableGenerator.h"
#include"DXRHelper.h"


using namespace Microsoft::WRL;
// We can include the correct library files here
// instead of in Visual Studio settings if we want
class DXCore
{
public:
	DXCore(HINSTANCE hInstance,
		const char* titleBarText,
		unsigned int windowWidth,
		unsigned int windowHeight,
		bool debugStats);

	//static members for os level processing
	static DXCore* DXCoreInstance;
	static LRESULT CALLBACK WindowProc(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam
	);

	LRESULT ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Initialization and game-loop related methods
	HRESULT InitWindow();
	void EnableShaderBasedValidation();
	HRESULT InitDirectX();
	HRESULT Run();
	void Quit();
	virtual void OnResize();

	// Pure virtual methods for setup and game functionality
	virtual HRESULT Init() = 0;
	virtual void Update(float deltaTime, float totalTime) = 0;
	virtual void Draw(float deltaTime, float totalTime) = 0;
	virtual void PopulateCommandList() =0;
	virtual void WaitForPreviousFrame()=0;
	virtual void MoveToNextFrame() = 0;

	// Convenience methods for handling mouse input, since we
	// can easily grab mouse input from OS-level messages
	virtual void OnMouseDown(WPARAM buttonState, int x, int y) { }
	virtual void OnMouseUp(WPARAM buttonState, int x, int y) { }
	virtual void OnMouseMove(WPARAM buttonState, int x, int y) { }
	virtual void OnMouseWheel(float wheelDelta, int x, int y) { }

protected:
	HINSTANCE	hInstance;		// The handle to the application
	HWND		hWnd;			// The handle to the window itself
	std::string titleBarText;	// Custom text in window's title bar
	bool		titleBarStats;	// Show extra stats in title bar?

	// Size of the window's client area
	unsigned int width;
	unsigned int height;

	static const int frameCount = 3;
	//pipeline objects
	ComPtr<ID3D12Device5> device;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<IDXGIAdapter3> adapter;
	//ComPtr<ID3D12Resource> renderTargets[2];
	ManagedResource renderTargets[frameCount];
	ComPtr<ID3D12CommandAllocator> commandAllocators[frameCount];
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12RootSignature> rootSignature;
	//ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	DescriptorHeapWrapper rtvDescriptorHeap;
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12GraphicsCommandList6> commandList;
	UINT rtvDescriptorSize;	

	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;


	//synchronization objects
	UINT frameIndex;
	HANDLE fenceEvent;
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValues[frameCount];

	void CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns);

	float totalTime;
	float deltaTime;

private:
	// Timing related data
	double perfCounterSeconds;
	__int64 startTime;
	__int64 currentTime;
	__int64 previousTime;

	// FPS calculation
	int fpsFrameCount;
	float fpsTimeElapsed;

	void UpdateTimer();			// Updates the timer for this frame
	void UpdateTitleBarStats();	// Puts debug info in the title bar

};

