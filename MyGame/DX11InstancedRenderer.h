#pragma once

#include "Definitions.h"
#include "IRenderer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <unordered_map>

#undef DrawText
#undef CreateFont

struct IDWriteFactory;
struct IWICImagingFactory;

using Microsoft::WRL::ComPtr;

// 인스턴싱 전용 렌더러.
// 쿼드 기하(4정점/6인덱스)는 한 번만 두고, 스프라이트/글리프마다 아래 인스턴스 레코드만 스트리밍한다.
// DrawIndexedInstanced 로 한 번에 N개를 찍고, 정점 변형(스케일/회전/이동)은 정점 셰이더가 담당.
struct SpriteInstance
{
    DirectX::XMFLOAT2 center; // 중심 위치(픽셀)
    DirectX::XMFLOAT2 size;   // 최종 크기(px, 텍스처크기*스케일)
    float             rot;    // 라디안
    DirectX::XMFLOAT2 uvMin;  // 아틀라스/텍스처 UV 사각형
    DirectX::XMFLOAT2 uvMax;
    DirectX::XMFLOAT4 color;  // 틴트(스프라이트=흰색, 폰트=텍스트색)
};

// 공유 쿼드의 정점(모서리 오프셋 + 모서리 UV 가중치)
struct QuadVertex
{
    DirectX::XMFLOAT2 corner; // -0.5 ~ 0.5
    DirectX::XMFLOAT2 uv;     // 0 ~ 1
};

class DX11InstancedRenderer : public IRenderer
{
public:
    DX11InstancedRenderer();
    ~DX11InstancedRenderer();

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
    void CreatePipeline();
    void BindPipeline();

    void PushInstance(ID3D11ShaderResourceView* srv, const SpriteInstance& inst);
    void FlushBatch();
    void DrawInstances(ID3D11ShaderResourceView* srv, const SpriteInstance* data, UINT count);

    void DrawTextImpl(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color);

private:
    ComPtr<ID3D11Device>           m_Device;
    ComPtr<ID3D11DeviceContext>    m_Context;
    ComPtr<IDXGISwapChain>         m_SwapChain;
    ComPtr<ID3D11RenderTargetView> m_RTV;

    ComPtr<ID3D11VertexShader>     m_VS;
    ComPtr<ID3D11PixelShader>      m_PS;
    ComPtr<ID3D11InputLayout>      m_InputLayout;
    ComPtr<ID3D11Buffer>           m_QuadVB;         // 공유 쿼드 정점(정적)
    ComPtr<ID3D11Buffer>           m_QuadIB;         // 공유 쿼드 인덱스(정적)
    ComPtr<ID3D11Buffer>           m_InstanceVB;     // 인스턴스 데이터(동적 링 버퍼)
    ComPtr<ID3D11Buffer>           m_ConstantBuffer;
    ComPtr<ID3D11BlendState>       m_BlendState;
    ComPtr<ID3D11SamplerState>     m_SamplerState;
    ComPtr<ID3D11RasterizerState>  m_RasterState;

    ComPtr<IDWriteFactory>         m_DWrite;
    ComPtr<IWICImagingFactory>     m_WIC;
    bool                           m_ComInitialized = false;
    bool                           m_AllowTearing = false;
    bool                           m_VSync = true;

    std::vector<SpriteInstance>    m_Batch;            // 연속 배치(비정렬 모드)
    ID3D11ShaderResourceView*      m_CurSRV = nullptr; // 현재 배치 텍스처(비소유)
    UINT                           m_InstUsed = 0;     // 링 버퍼에 이번 프레임 쓴 인스턴스 수

    // 텍스처 버킷팅(정렬) 모드: 프레임 전체 인스턴스를 SRV별로 모아 SRV당 draw 1회.
    // 제출 순서를 재정렬하므로 불투명/동일 z-레이어 가정. config [Stress] Sort=1 로 활성.
    bool                           m_SortByTexture = false;
    std::unordered_map<ID3D11ShaderResourceView*, std::vector<SpriteInstance>> m_Buckets;
};
