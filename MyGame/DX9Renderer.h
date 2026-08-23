#pragma once

#include "Definitions.h"
#include "IRenderer.h"

class DX9Renderer : public IRenderer
{
public:
    DX9Renderer();
    ~DX9Renderer();

    void Init(HWND hWnd) override;
    void Release() override;

    void RenderStart() override;
    void RenderEnd() override;

    void SpriteRenderStart() override;
    void SpriteRenderEnd() override;

    void FontRenderStart() override;
    void FontRenderEnd() override;

    TextureHandle CreateTexture(const std::wstring& path) override;
    void ReleaseTexture(TextureHandle& handle) override;

    FontHandle CreateFont(const std::wstring& name, int size) override;
    void ReleaseFont(FontHandle& handle) override;

	void DrawSprite(TextureHandle* handle, float x, float y, float sx, float sy, float rot, bool flip) override;
    void DrawText(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color) override;
    void DrawText(FontHandle* handle, const std::string& text, int x, int y, const Color& color) override;

private:
    LPDIRECT3D9         m_D3D = nullptr;
    LPDIRECT3DDEVICE9   m_Device = nullptr;
    LPD3DXSPRITE        m_Sprite = nullptr;
    LPD3DXFONT			m_SmallFont = nullptr;
    LPD3DXFONT			m_LargeFont = nullptr;
};