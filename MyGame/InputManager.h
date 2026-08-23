#pragma once
#include "Definitions.h"
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")



class InputManager : public Singleton<InputManager>
{

public:
	InputManager() {

	}
	~InputManager() {
		Release();
	}


public:
	HRESULT Init(HWND hwnd, int ScreenWidth, int ScreenHeight);

	bool ReadKeyboard();

	bool ReadMouse();

	void ProcessInput();

	void GetMousePosition(int &x, int &y);

	int IsKeyDown(byte keyCode);

	void Release();

private:
	IDirectInput8* m_pDirectInput;
	IDirectInputDevice8* m_pKeyboard;
	IDirectInputDevice8* m_pMouse;


	unsigned char m_keyboardState[256];
	DIMOUSESTATE m_mouseState;

	int m_screenWidth, m_screenHeight;
	int m_mouseX, m_mouseY;

private:
	static InputManager* m_pInstance;

};