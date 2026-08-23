#pragma once

#include "Definitions.h"
#include "IRenderer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>

// windows.h(winuser)가 정의하는 DrawText/CreateFont 매크로 제거.
// 이 헤더보다 앞서 d3dx9/windows 계열 헤더가 include 되면 매크로가 살아있어
// IRenderer 의 DrawText/CreateFont 멤버 선언을 오염시키므로 방어적으로 undef.
#undef DrawText
#undef CreateFont

// DirectWrite / WIC (COM) - 순수 Win32/DX 스택. DirectXTK 의존 없음.
struct IDWriteFactory;
struct IWICImagingFactory;

using Microsoft::WRL::ComPtr;

// 스프라이트/글리프 공용 정점. 픽셀 좌표 + UV + 정점 컬러(틴트).
struct SpriteVertex
{
    DirectX::XMFLOAT2 pos; // 화면 픽셀 좌표(좌상단 원점, Y-down). VS에서 직교투영.
    DirectX::XMFLOAT2 uv;
    DirectX::XMFLOAT4 color;
};

class DX11Renderer : public IRenderer
{
public:
    DX11Renderer();
    ~DX11Renderer();

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
    void CreateDevice(HWND hWnd);
    void CreatePipeline();  // 셰이더/레이아웃/스테이트/버퍼 생성
    void BindPipeline();    // 프레임 시작 시 파이프라인 상태 바인딩

    // 배치: 같은 SRV 쿼드를 모아 드로우콜 1회로 묶음
    void PushQuad(ID3D11ShaderResourceView* srv, const SpriteVertex v[6]);
    void FlushBatch();

    // DrawText 공용 구현(내부에서 글리프 아틀라스 갱신 + 쿼드 push)
    void DrawTextImpl(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color);

private:
    ComPtr<ID3D11Device>           m_Device;
    ComPtr<ID3D11DeviceContext>    m_Context;
    ComPtr<IDXGISwapChain>         m_SwapChain;
    ComPtr<ID3D11RenderTargetView> m_RTV;

    // 직접 작성한 스프라이트 파이프라인
    ComPtr<ID3D11VertexShader>     m_VS;
    ComPtr<ID3D11PixelShader>      m_PS;
    ComPtr<ID3D11InputLayout>      m_InputLayout;
    ComPtr<ID3D11Buffer>           m_VertexBuffer;   // 동적 링 버퍼
    ComPtr<ID3D11Buffer>           m_ConstantBuffer; // 직교투영 행렬
    ComPtr<ID3D11BlendState>       m_BlendState;
    ComPtr<ID3D11SamplerState>     m_SamplerState;
    ComPtr<ID3D11RasterizerState>  m_RasterState;

    // COM 팩토리(폰트/이미지)
    ComPtr<IDWriteFactory>         m_DWrite;
    ComPtr<IWICImagingFactory>     m_WIC;
    bool                           m_ComInitialized = false;
    bool                           m_AllowTearing = false; // 가변 리프레시(VSync 해제) 하드웨어 지원 여부
    bool                           m_VSync = true;         // config.ini [Graphics] VSync (기본 켜짐)

    // 배치 상태
    std::vector<SpriteVertex>      m_Batch;          // 현재 배치 CPU 정점
    ID3D11ShaderResourceView*      m_CurSRV = nullptr; // 현재 배치 텍스처(비소유)
    UINT                           m_VBUsed = 0;     // 링 버퍼에 이번 프레임 쓴 정점 수
};
