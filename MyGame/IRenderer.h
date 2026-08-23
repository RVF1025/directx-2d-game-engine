#pragma once
#include "Definitions.h"

#undef DrawText
#undef CreateFont

struct Color
{
    float r, g, b, a;
};

struct TextureHandle
{
    void* texture = nullptr; // LPDIRECT3DTEXTURE9 or ID3D11ShaderResourceView*
    int width = 0;           // 논리(서브이미지) 크기
    int height = 0;

    // 아틀라스 서브렉트. 기본 = 텍스처 전체.
    // DX11 계열은 정규화 UV(u0..v1), DX9 은 소스 픽셀 좌표(srcX/srcY + width/height)를 사용.
    float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
    int   srcX = 0, srcY = 0;
    bool  isSub = false;     // true 면 공유 SRV 의 부분 영역(소유하지 않음)
};

struct FontHandle
{
    void* font = nullptr; // LPD3DXFONT or IDWriteTextFormat*
    int size = 0;
};

class IRenderer
{
public:
    virtual ~IRenderer() {}

    virtual void Init(HWND hWnd) = 0;
    virtual void Release() = 0;

    virtual void RenderStart() = 0;
    virtual void RenderEnd() = 0;

    virtual void SpriteRenderStart() = 0;
    virtual void SpriteRenderEnd() = 0;

    virtual void FontRenderStart() = 0;
    virtual void FontRenderEnd() = 0;

    virtual TextureHandle CreateTexture(const std::wstring& path) = 0;
    virtual void ReleaseTexture(TextureHandle& handle) = 0;

	virtual FontHandle CreateFont(const std::wstring& name, int size) = 0;
    virtual void ReleaseFont(FontHandle& handle) = 0;

    virtual void DrawSprite(TextureHandle* handle, float x, float y, float sx, float sy, float rot, bool flip) = 0;
    virtual void DrawText(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color) = 0;
    virtual void DrawText(FontHandle* handle, const std::string& text, int x, int y, const Color& color) = 0;
};