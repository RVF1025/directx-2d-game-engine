#include "Window.h"

Window::Window()
{
}

Window::~Window()
{
    if (m_hWnd)
        DestroyWindow(m_hWnd);
}

bool Window::Create(const std::wstring& title, int width, int height)
{
    WNDCLASSEX wc =
    {
        sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, Window::WndProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, LoadCursor(nullptr, IDC_ARROW), NULL, NULL,
        title.c_str(), NULL
    };

    RegisterClassEx(&wc);

    m_hWnd = CreateWindow(title.c_str(), title.c_str(),
        WS_OVERLAPPEDWINDOW, 300, 0, width, height,
        NULL, NULL, wc.hInstance, NULL);

    return m_hWnd != nullptr;
}

void Window::Show()
{
    ShowWindow(m_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hWnd);
}

bool Window::MsgLoop()
{
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}