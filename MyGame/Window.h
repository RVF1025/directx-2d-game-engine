#pragma once
#include <Windows.h>
#include <string>

class Window
{
public:
    Window();
    ~Window();

	bool Create(const std::wstring& title, int width, int height);
    void Show();

    bool MsgLoop();
    HWND GetHWnd() const { return m_hWnd; }

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND        m_hWnd = nullptr;
};
