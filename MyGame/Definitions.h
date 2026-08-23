#pragma once

#pragma warning (disable : 4005)
#pragma warning (disable : 4251)

//#ifdef UNICODE
//#pragma comment(linker, "/entry:wWinMainCRTStartup /subsystem:console") 
//#else
//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") 
//#endif

#include <d3d9.h>
#include <d3dx9.h>

#include <d3d11.h>
#include <dxgi.h>
#include <directxmath.h>

#include <string>

#define WIN32_LEAN_AND_MEAN            
#include <windows.h>

#include "Singleton.h"
#include "MyECS.h"

#define		SCREEN_WIDTH			800						
#define		SCREEN_HEIGHT			800

#define DIRECTINPUT_VERSION 0x0800

