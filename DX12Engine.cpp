#define _CRTDBG_MAP_ALLOC
#define NOMINMAX
#include <Windows.h>
#include "Game.h"
#include<crtdbg.h>
// --------------------------------------------------------
// Entry point for a graphical (non-console) Windows application
// --------------------------------------------------------
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,		// The handle to this app's instance
	_In_opt_ HINSTANCE hPrevInstance,	// A handle to the previous instance of the app (always NULL)
	_In_ LPSTR lpCmdLine,			// Command line params
	_In_ int nCmdShow)				// How the window should be shown (we ignore this)
{
#if defined(DEBUG) | defined(_DEBUG)
	// Enable memory leak detection as a quick and dirty
	// way of determining if we forgot to clean something up
	//  - You may want to use something more advanced, like Visual Leak Detector
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Ensure "Current Directory" (relative path) is always the .exe's folder
	{
		// Get the real, full path to this executable, end the string before
		// the filename itself and then set that as the current directory
		char currentDir[1024] = {};
		GetModuleFileNameA(0, currentDir, 1024);
		char* lastSlash = strrchr(currentDir, '\\');
		if (lastSlash)
		{
			*lastSlash = 0; // End the string at the last slash character
			SetCurrentDirectoryA(currentDir);
		}
	}

	// Create the Game object using
	// the app handle we got from WinMain
	Game dxGame(hInstance);

	// Result variable for function calls below
	HRESULT hr = S_OK;

	// Attempt to create the window for our program, and
	// exit early if something failed
	hr = dxGame.InitWindow();
	if (FAILED(hr)) return hr;

	// Attempt to initialize DirectX, and exit
	// early if something failed
	hr = dxGame.InitDirectX();
	if (FAILED(hr)) return hr;

	// Begin the message and game loop, and then return
	// whatever we get back once the game loop is over
	return dxGame.Run();
}