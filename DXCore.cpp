#include "DXCore.h"

// Define the static instance variable so our OS-level 
// message handling function below can talk to our object
DXCore* DXCore::DXCoreInstance = nullptr;

DXCore::DXCore(HINSTANCE hInstance, const char* titleBarText, unsigned int windowWidth, 
	unsigned int windowHeight, bool debugStats)
{
	DXCoreInstance = this;
	this->hInstance = hInstance;
	this->titleBarText = titleBarText;
	this->width = windowWidth;
	this->height = windowHeight;
	this->titleBarStats = debugStats;

	fpsFrameCount = 0;
	fpsTimeElapsed = 0;
	frameIndex = 0;
	viewport.Height = static_cast<FLOAT>(height);
	viewport.Width = static_cast<FLOAT>(width);
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX=0;
	viewport.TopLeftY = 0;

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = static_cast<LONG>(width);
	scissorRect.bottom = static_cast<LONG>(height);
	rtvDescriptorSize = 0;

	__int64 perfFreq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&perfFreq);
	perfCounterSeconds = 1 / (double)perfFreq;
}

LRESULT DXCore::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DXCoreInstance->ProcessMessage(hWnd, uMsg, wParam, lParam);
}

LRESULT DXCore::ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Check the incoming message and handle any we care about
	switch (uMsg)
	{
		// This is the message that signifies the window closing
	case WM_DESTROY:
		PostQuitMessage(0); // Send a quit message to our own program
		return 0;

		// Prevent beeping when we "alt-enter" into fullscreen
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);

		// Prevent the overall window from becoming too small
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

		// Sent when the window size changes
	case WM_SIZE:
		// Don't adjust anything when minimizing,
		// since we end up with a width/height of zero
		// and that doesn't play well with DirectX
		if (wParam == SIZE_MINIMIZED)
			return 0;

		// Save the new client area dimensions.
		width = LOWORD(lParam);
		height = HIWORD(lParam);

		// If DX is initialized, resize 
		// our required buffers
		if (device)
			OnResize();

		return 0;

		// Mouse button being pressed (while the cursor is currently over our window)
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse button being released (while the cursor is currently over our window)
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Cursor moves over the window (or outside, while we're currently capturing it)
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

		// Mouse wheel is scrolled
	case WM_MOUSEWHEEL:
		OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}

	// Let Windows handle any messages we're not touching
	return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

HRESULT DXCore::InitWindow()
{
	// appropriate window class struct
	WNDCLASS wndClass = {}; // Zero out the memory
	wndClass.style = CS_HREDRAW | CS_VREDRAW;	// Redraw on horizontal or vertical movement/adjustment
	wndClass.lpfnWndProc = DXCore::WindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;						// Our app's handle
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);	// Default icon
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);		// Default arrow cursor
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = L"Direct3DWindowClass";

	// Attempt to register the window class we've defined
	if (!RegisterClass(&wndClass))
	{
		// Get the most recent error
		DWORD error = GetLastError();

		// If the class exists, that's actually fine.  Otherwise,
		// we can't proceed with the next step.
		if (error != ERROR_CLASS_ALREADY_EXISTS)
			return HRESULT_FROM_WIN32(error);
	}

	// Adjust the width and height so the "client size" matches
	// the width and height given (the inner-area of the window)
	RECT clientRect;
	SetRect(&clientRect, 0, 0, width, height);
	AdjustWindowRect(
		&clientRect,
		WS_OVERLAPPEDWINDOW,	// Has a title bar, border, min and max buttons, etc.
		false);					// No menu bar

	// Center the window to the screen
	RECT desktopRect;
	GetClientRect(GetDesktopWindow(), &desktopRect);
	int centeredX = (desktopRect.right / 2) - (clientRect.right / 2);
	int centeredY = (desktopRect.bottom / 2) - (clientRect.bottom / 2);

	// Actually ask Windows to create the window itself
	// using our settings so far.  This will return the
	// handle of the window, which we'll keep around for later
	hWnd = CreateWindowW(
		wndClass.lpszClassName,
		(LPWSTR)titleBarText.c_str(),
		WS_OVERLAPPEDWINDOW,
		centeredX,
		centeredY,
		clientRect.right - clientRect.left,	// Calculated width
		clientRect.bottom - clientRect.top,	// Calculated height
		0,			// No parent window
		0,			// No menu
		hInstance,	// The app's handle
		0);			// No other windows in our application

	// Ensure the window was created properly
	if (hWnd == NULL)
	{
		DWORD error = GetLastError();
		return HRESULT_FROM_WIN32(error);
	}

	// The window exists but is not visible yet
	// We need to tell Windows to show it, and how to show it
	ShowWindow(hWnd, SW_SHOW);


	// Return an "everything is ok" HRESULT value
	return S_OK;
}

HRESULT DXCore::InitDirectX()
{

	// Enable the D3D12 debug layer.
	{

		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}


	ComPtr<IDXGIFactory4> factory;
	auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (FAILED(hr)) return hr;

	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		hr = D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)
		);

		ThrowIfFailed(hardwareAdapter.As(&adapter));
		if (FAILED(hr)) return hr;
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

	if (FAILED(hr)) return hr;

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameCount;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> pswapChain;
	hr = factory->CreateSwapChain(
		commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		&swapChainDesc,
		&pswapChain
	);

	if (FAILED(hr)) return hr;

	hr = pswapChain.As(&this->swapChain);

	if (FAILED(hr)) return hr;

	// This sample does not support fullscreen transitions.
	hr = (factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	if (FAILED(hr)) return hr;


	// Return an "everything is ok" HRESULT value
	return S_OK;
}

HRESULT DXCore::Run()
{
	// Grab the start time now that
// the game loop is running
	__int64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)& now);
	startTime = now;
	currentTime = now;
	previousTime = now;

	// Give subclass a chance to initialize
	Init();

	// Our overall game and message loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Determine if there is a message waiting
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate and dispatch the message
			// to our custom WindowProc function
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else
		{
			// Update timer and title bar (if necessary)
			UpdateTimer();
			if (titleBarStats)
				UpdateTitleBarStats();

			// The game loop
			Update(deltaTime, totalTime);
			Draw(deltaTime, totalTime);
		}
	}

	// We'll end up here once we get a WM_QUIT message,
	// which usually comes from the user closing the window
	return (HRESULT)msg.wParam;
}

void DXCore::Quit()
{
	PostMessage(this->hWnd, WM_CLOSE, NULL, NULL);
}

void DXCore::OnResize()
{
}

void DXCore::CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
{
	// Our temp console info struct
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// Get the console info and set the number of lines
	AllocConsole();
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = bufferLines;
	coninfo.dwSize.X = bufferColumns;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = windowColumns;
	rect.Bottom = windowLines;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);

	FILE* stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stdout);
	freopen_s(&stream, "CONOUT$", "w", stderr);

	// Prevent accidental console window close
	HWND consoleHandle = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(consoleHandle, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
}

void DXCore::UpdateTimer()
{
	// Grab the current time
	__int64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)& now);
	currentTime = now;

	// Calculate delta time and clamp to zero
	//  - Could go negative if CPU goes into power save mode 
	//    or the process itself gets moved to another core
	deltaTime = max((float)((currentTime - previousTime) * perfCounterSeconds), 0.0f);

	// Calculate the total time from start to now
	totalTime = (float)((currentTime - startTime) * perfCounterSeconds);

	// Save current time for next frame
	previousTime = currentTime;
}

void DXCore::UpdateTitleBarStats()
{
	fpsFrameCount++;

	// Only calc FPS and update title bar once per second
	float timeDiff = totalTime - fpsTimeElapsed;
	if (timeDiff < 1.0f)
		return;

	// How long did each frame take?  (Approx)
	float mspf = 1000.0f / (float)fpsFrameCount;

	// Quick and dirty title bar text (mostly for debugging)
	std::ostringstream output;
	output.precision(6);
	output << titleBarText <<
		"    Width: " << width <<
		"    Height: " << height <<
		"    FPS: " << fpsFrameCount <<
		"    Frame Time: " << mspf << "ms";

	// Append the version of DirectX the app is using
	/*switch (dxFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_11_1: output << "    DX 11.1"; break;
	case D3D_FEATURE_LEVEL_11_0: output << "    DX 11.0"; break;
	case D3D_FEATURE_LEVEL_10_1: output << "    DX 10.1"; break;
	case D3D_FEATURE_LEVEL_10_0: output << "    DX 10.0"; break;
	case D3D_FEATURE_LEVEL_9_3:  output << "    DX 9.3";  break;
	case D3D_FEATURE_LEVEL_9_2:  output << "    DX 9.2";  break;
	case D3D_FEATURE_LEVEL_9_1:  output << "    DX 9.1";  break;
	default:                     output << "    DX ???";  break;
	}*/

	// Actually update the title bar and reset fps data
	SetWindowTextW(hWnd, (LPCWSTR)output.str().c_str());
	fpsFrameCount = 0;
	fpsTimeElapsed += 1.0f;
}
