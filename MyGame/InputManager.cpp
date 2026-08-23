#include"InputManager.h"

InputManager* InputManager::m_pInstance = nullptr;
HRESULT InputManager::Init(HWND hwnd, int ScreenWidth, int ScreenHeight)
{

	m_screenWidth = ScreenWidth;

	m_screenHeight = ScreenHeight;

	if (FAILED(DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&m_pDirectInput, NULL))) { // Directinput8를 생성합니다.
		return E_FAIL;
	}
	if (FAILED(m_pDirectInput->CreateDevice(GUID_SysKeyboard, &m_pKeyboard, NULL))) {// m_pDinput에 디바이스를 생성합니다.
		return E_FAIL;
	}

	if (FAILED(m_pKeyboard->SetDataFormat(&c_dfDIKeyboard))) {
		return E_FAIL;
	}

	/*
	DISCL_FOREGROUND
	에플리케이션이 포커스를 받고 있을때만 디바이스가 인풋을 받는다

	DISCL_BACKGROUND
	에플리케이션이 포커스를 받고 있든지 아니든지 관계없이 계속 디바이스가 인풋을 받는다.

	DISCL_EXCLUSIVE
	접근이 요청되는 동안 디바이스에 독점적인 접근을 할수 있는 어떤 인스턴스도 없을때

	DISCL_NONEXCLUSIVE
	디바이스에 접근하는 것을 다른 에플리케이션이 방해하지 않을때만

	*/
	//DISCL_NONEXCLUSIVE 독점하지 아
	if (FAILED(m_pKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE))) {
		return E_FAIL;
	}

	if (FAILED(m_pDirectInput->CreateDevice(GUID_SysMouse, &m_pMouse, NULL))) {// m_pDinput에 디바이스를 생성합니다.
		return E_FAIL;
	}

	if (FAILED(m_pMouse->SetDataFormat(&c_dfDIMouse))) {
		return E_FAIL;
	}
	if (FAILED(m_pMouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE))) {
		return E_FAIL;
	}

	ReadKeyboard();
	ReadMouse();
	ProcessInput();
	return S_OK;
}

bool InputManager::ReadKeyboard() {
	HRESULT result;

	result = m_pKeyboard->GetDeviceState(sizeof(m_keyboardState), (LPVOID)&m_keyboardState);
	if (FAILED(result)) { // If the keyboard lost focus or was not acquired then try to get control back. 
		if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED)) {
			m_pKeyboard->Acquire();
		}
		else {
			return false;
		}
	} return true;
}


bool InputManager::ReadMouse()
{
	HRESULT result;

	if (m_pMouse == nullptr)
		return true;
	// Read the mouse device.
	result = m_pMouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&m_mouseState);
	if (FAILED(result))
	{
		// If the mouse lost focus or was not acquired then try to get control back.
		if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED))
		{
			m_pMouse->Acquire();
		}
		else
		{
			return false;
		}
	}

	return true;
}


void InputManager::ProcessInput() {
	// Update the location of the mouse cursor based on the change of the mouse location during the frame. 
	m_mouseX += m_mouseState.lX; m_mouseY += m_mouseState.lY; //
	// Ensure the mouse location doesn't exceed the screen width or height. 
	if (m_mouseX < 0) { m_mouseX = 0; } if (m_mouseY < 0) { m_mouseY = 0; }
	if (m_mouseX > m_screenWidth) {
		m_mouseX = m_screenWidth;
	}
	if (m_mouseY > m_screenHeight) { m_mouseY = m_screenHeight; }
	return;
}

void InputManager::GetMousePosition(int & x, int & y)
{


	x = m_mouseX;
	y = m_mouseY;


}




int InputManager::IsKeyDown(byte keyCode) {

	ReadKeyboard();
	ProcessInput();
	return m_keyboardState[keyCode] & 0x80;
}


void InputManager::Release() {


	if (m_pKeyboard != nullptr) {
		m_pKeyboard->Unacquire();
	}

	if (m_pKeyboard != nullptr) {
		m_pKeyboard->Release();
	}


	if (m_pMouse != nullptr) {
		m_pMouse->Unacquire();
	}

	if (m_pMouse != nullptr) {
		m_pMouse->Release();
	}


	if (m_pDirectInput != nullptr) {
		m_pDirectInput->Release();
	}
}