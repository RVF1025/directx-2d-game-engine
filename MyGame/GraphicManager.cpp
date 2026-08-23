#include "GraphicManager.h"
#include "DX9Renderer.h"
#include "DX11Renderer.h"
#include "DX11InstancedRenderer.h"

GraphicManager::GraphicManager()
{
}

GraphicManager::~GraphicManager()
{
}

void GraphicManager::Init(RenderAPI api, HWND hWnd)
{
    switch (api)
    {
    case RenderAPI::DX9:
        m_Renderer = std::make_unique<DX9Renderer>();
        break;
    case RenderAPI::DX11:
        m_Renderer = std::make_unique<DX11Renderer>();
        break;
    case RenderAPI::DX11Instanced:
        m_Renderer = std::make_unique<DX11InstancedRenderer>();
        break;
    }

    m_Renderer->Init(hWnd);
}

void GraphicManager::RenderStart()
{
    m_Renderer->RenderStart();
}

void GraphicManager::RenderEnd()
{
    m_Renderer->RenderEnd();
}

void GraphicManager::Release()
{
    if (m_Renderer)
    {
        m_Renderer->Release();
        m_Renderer.reset();
    }
}