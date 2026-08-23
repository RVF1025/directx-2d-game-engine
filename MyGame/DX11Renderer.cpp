#include "DX11Renderer.h"

#include <d3dcompiler.h>
#include <dwrite.h>
#include <wincodec.h>
#include <objbase.h>
#include <dxgi1_5.h>   // IDXGIFactory5, ALLOW_TEARING

#include <unordered_map>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using namespace DirectX;

namespace
{
    // 화면 픽셀 좌표를 클립 공간으로 보내는 직교투영 + 텍스처 * 정점컬러.
    // 좌상단 원점 / Y-down 을 그대로 쓰기 위해 offcenter ortho 를 CPU 에서 만들어 상수버퍼로 전달.
    const char* kShaderSrc = R"(
cbuffer CB : register(b0)
{
    row_major float4x4 gProj;
};

struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = mul(float4(i.pos, 0.0f, 1.0f), gProj);
    o.uv  = i.uv;
    o.col = i.col;
    return o;
}

Texture2D    gTex : register(t0);
SamplerState gSmp : register(s0);

float4 PSMain(VSOut i) : SV_Target
{
    return gTex.Sample(gSmp, i.uv) * i.col;
}
)";

    constexpr UINT kMaxVerts = 240000; // 동적 링 버퍼 정점 용량 (약 4만 쿼드, 32B/정점 ≈ 7.6MB)
    constexpr UINT kAtlasSize = 1024; // 글리프 아틀라스 한 변 (px)

    // config.ini [Graphics] VSync 읽기 (기본 1=켜짐). GetPrivateProfileInt 는 절대 경로가
    // 필요하므로 현재 작업 디렉터리 기준으로 조합. <filesystem> 을 끌어오지 않으려 Win32 API 사용.
    bool ReadVSyncConfig()
    {
        wchar_t dir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, dir);
        std::wstring path = std::wstring(dir) + L"\\Data\\Config\\config.ini";
        return GetPrivateProfileIntW(L"Graphics", L"VSync", 1, path.c_str()) != 0;
    }
}

// 아틀라스에 캐싱된 단일 글리프 정보
struct GlyphInfo
{
    float u0, v0, u1, v1; // 아틀라스 UV
    int   width, height;  // 픽셀 크기
    int   offsetX;        // 원점(baseline) 기준 좌측 오프셋
    int   offsetY;        // baseline 기준 상단 오프셋(위쪽이 음수)
    float advance;        // 다음 글자까지 진행폭(px)
};

// FontHandle.font 가 가리키는 실체. DirectWrite 폰트페이스 + 온디맨드 글리프 아틀라스.
struct FontAtlas
{
    ComPtr<IDWriteFontFace>           face;
    ComPtr<ID3D11Texture2D>           tex;
    ComPtr<ID3D11ShaderResourceView>  srv;

    float emSize = 0.f;
    float ascent = 0.f;   // baseline 까지의 높이(px)

    // 셸프 패킹 커서
    int penX = 1, penY = 1, rowHeight = 0;

    std::unordered_map<wchar_t, GlyphInfo> glyphs;
};

DX11Renderer::DX11Renderer() {}

DX11Renderer::~DX11Renderer() {}

void DX11Renderer::Init(HWND hWnd)
{
    // WIC 는 COM 아파트먼트를 요구. 이미 초기화돼 있으면(다른 서브시스템) 그대로 사용.
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_ComInitialized = SUCCEEDED(hrCo); // S_FALSE/RPC_E_CHANGED_MODE 여도 WIC 사용 가능

    m_VSync = ReadVSyncConfig();

    CreateDevice(hWnd);
    CreatePipeline();
    BindPipeline();   // 정적 파이프라인 상태는 한 번만 바인딩(컨텍스트는 상태 머신 — 프레임마다 유지됨)

    // DirectWrite / WIC 팩토리
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_DWrite.GetAddressOf()));

    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_WIC.GetAddressOf()));

    m_Batch.reserve(kMaxVerts);
}

void DX11Renderer::Release()
{
    m_Batch.clear();
    if (m_Context) m_Context->ClearState();

    // ComPtr 들이 자동 해제. WIC/DWrite 도 해제 후 COM 정리.
    m_WIC.Reset();
    m_DWrite.Reset();

    if (m_ComInitialized)
    {
        CoUninitialize();
        m_ComInitialized = false;
    }
}

void DX11Renderer::CreateDevice(HWND hWnd)
{
    // 가변 리프레시(테어링 허용) 지원 여부 조회 — 플립 모델에서 VSync 해제하려면 필수
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory5))))
        {
            BOOL tearing = FALSE;
            if (SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))))
                m_AllowTearing = (tearing == TRUE);
        }
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;                                   // 더블 버퍼(플립 모델)
    sd.BufferDesc.Width = SCREEN_WIDTH;
    sd.BufferDesc.Height = SCREEN_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;                             // 플립 모델은 MSAA 불가
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_SwapChain, &m_Device, nullptr, &m_Context);

    // 디버그 레이어 미설치 등으로 실패하면 플래그 없이 재시도
    if (FAILED(hr))
    {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &m_SwapChain, &m_Device, nullptr, &m_Context);
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RTV);

    D3D11_VIEWPORT vp{};
    vp.Width = (FLOAT)SCREEN_WIDTH;
    vp.Height = (FLOAT)SCREEN_HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_Context->RSSetViewports(1, &vp);
}

void DX11Renderer::CreatePipeline()
{
    // 1) 셰이더 런타임 컴파일 (D3DCompile - 파이프라인을 직접 소유)
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
        "VSMain", "vs_4_0", compileFlags, 0, &vsBlob, &errBlob);
    D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr,
        "PSMain", "ps_4_0", compileFlags, 0, &psBlob, &errBlob);

    m_Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VS);
    m_Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PS);

    // 2) 인풋 레이아웃
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    m_Device->CreateInputLayout(layout, _countof(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_InputLayout);

    // 3) 동적 정점 버퍼 (링 버퍼로 WRITE_NO_OVERWRITE 갱신)
    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = sizeof(SpriteVertex) * kMaxVerts;
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_Device->CreateBuffer(&vbd, nullptr, &m_VertexBuffer);

    // 4) 상수 버퍼 - 직교투영(좌상단 원점, Y-down)
    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
        0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMFLOAT4X4 projStored;
    XMStoreFloat4x4(&projStored, proj);

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(XMFLOAT4X4);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cbInit{ &projStored, 0, 0 };
    m_Device->CreateBuffer(&cbd, &cbInit, &m_ConstantBuffer);

    // 5) 블렌드 스테이트 - 스트레이트 알파
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_Device->CreateBlendState(&bd, &m_BlendState);

    // 6) 샘플러 - 선형 / 클램프
    D3D11_SAMPLER_DESC smp{};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    m_Device->CreateSamplerState(&smp, &m_SamplerState);

    // 7) 래스터라이저 - 컬링 없음(2D)
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = TRUE;
    m_Device->CreateRasterizerState(&rs, &m_RasterState);
}

void DX11Renderer::BindPipeline()
{
    UINT stride = sizeof(SpriteVertex);
    UINT offset = 0;

    m_Context->IASetInputLayout(m_InputLayout.Get());
    m_Context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
    m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_Context->VSSetShader(m_VS.Get(), nullptr, 0);
    m_Context->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
    m_Context->PSSetShader(m_PS.Get(), nullptr, 0);
    m_Context->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    float blendFactor[4] = { 0, 0, 0, 0 };
    m_Context->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xffffffff);
    m_Context->RSSetState(m_RasterState.Get());
}

void DX11Renderer::RenderStart()
{
    float clear[4] = { 0.f, 0.f, 0.f, 1.f };
    m_Context->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);
    m_Context->ClearRenderTargetView(m_RTV.Get(), clear);

    // 정적 파이프라인 상태(셰이더/레이아웃/버퍼/스테이트)는 Init 에서 한 번만 바인딩했고
    // 컨텍스트가 그대로 유지하므로 매 프레임 재바인딩하지 않는다. RTV/Clear 만 프레임마다.

    // 새 프레임: 링 버퍼 오프셋/배치 초기화
    m_VBUsed = 0;
    m_CurSRV = nullptr;
    m_Batch.clear();
}

void DX11Renderer::RenderEnd()
{
    FlushBatch();                 // 혹시 남은 배치 정리(안전)

    if (m_VSync)
    {
        m_SwapChain->Present(1, 0);   // 수직동기 — 주사율에 맞춰 표시(테어링 없음)
    }
    else
    {
        // VSync 해제. 플립 모델에서 실제로 제한을 풀려면 ALLOW_TEARING 플래그가
        // 함께 있어야 DWM 합성 주사율 캡을 우회한다(하드웨어 지원 시).
        UINT presentFlags = m_AllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        m_SwapChain->Present(0, presentFlags);
    }
}

void DX11Renderer::SpriteRenderStart() { /* 배치 파이프라인은 RenderStart에서 이미 준비됨 */ }
void DX11Renderer::SpriteRenderEnd()   { FlushBatch(); }
void DX11Renderer::FontRenderStart()   {}
void DX11Renderer::FontRenderEnd()     { FlushBatch(); }

// ---- 배치 ----------------------------------------------------------------

void DX11Renderer::PushQuad(ID3D11ShaderResourceView* srv, const SpriteVertex v[6])
{
    // 텍스처가 바뀌면 지금까지 모은 배치를 먼저 드로우
    if (srv != m_CurSRV)
    {
        FlushBatch();
        m_CurSRV = srv;
    }

    // 링 버퍼 용량 초과 예상 시 강제 flush (드문 케이스)
    if (m_Batch.size() + 6 > kMaxVerts)
        FlushBatch();

    for (int i = 0; i < 6; ++i)
        m_Batch.push_back(v[i]);
}

void DX11Renderer::FlushBatch()
{
    if (m_Batch.empty() || m_CurSRV == nullptr)
    {
        m_Batch.clear();
        return;
    }

    UINT count = (UINT)m_Batch.size();

    // 링 버퍼 끝을 넘어가면 DISCARD 로 앞에서부터 다시 씀
    if (m_VBUsed + count > kMaxVerts)
        m_VBUsed = 0;

    D3D11_MAP mapType = (m_VBUsed == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    m_Context->Map(m_VertexBuffer.Get(), 0, mapType, 0, &mapped);
    memcpy(reinterpret_cast<SpriteVertex*>(mapped.pData) + m_VBUsed,
        m_Batch.data(), count * sizeof(SpriteVertex));
    m_Context->Unmap(m_VertexBuffer.Get(), 0);

    m_Context->PSSetShaderResources(0, 1, &m_CurSRV);
    m_Context->Draw(count, m_VBUsed);

    m_VBUsed += count;
    m_Batch.clear();
}

// ---- 스프라이트 ----------------------------------------------------------

void DX11Renderer::DrawSprite(TextureHandle* handle, float x, float y, float sx, float sy, float rot, bool flip)
{
    if (!handle || !handle->texture)
        return;

    auto srv = static_cast<ID3D11ShaderResourceView*>(handle->texture);

    float w = handle->width * sx;
    float h = handle->height * sy;
    float hw = w * 0.5f;
    float hh = h * 0.5f;

    // 중심 원점 기준 4모서리 (DXTK Draw 의 origin=size/2 동작과 동일)
    XMFLOAT2 corners[4] =
    {
        { -hw, -hh }, { hw, -hh }, { hw, hh }, { -hw, hh }
    };

    float rad = XMConvertToRadians(rot);
    float c = cosf(rad);
    float s = sinf(rad);

    XMFLOAT2 p[4];
    for (int i = 0; i < 4; ++i)
    {
        p[i].x = x + corners[i].x * c - corners[i].y * s;
        p[i].y = y + corners[i].x * s + corners[i].y * c;
    }

    // 아틀라스 서브렉트 UV(기본 0..1). flip 이면 u 를 뒤집는다.
    float u0 = flip ? handle->u1 : handle->u0;
    float u1 = flip ? handle->u0 : handle->u1;
    float v0 = handle->v0;
    float v1 = handle->v1;

    XMFLOAT4 white{ 1.f, 1.f, 1.f, 1.f };

    SpriteVertex quad[6] =
    {
        { p[0], { u0, v0 }, white },
        { p[1], { u1, v0 }, white },
        { p[2], { u1, v1 }, white },
        { p[0], { u0, v0 }, white },
        { p[2], { u1, v1 }, white },
        { p[3], { u0, v1 }, white },
    };

    PushQuad(srv, quad);
}

// ---- 폰트 (DirectWrite 온디맨드 글리프 아틀라스) --------------------------

FontHandle DX11Renderer::CreateFont(const std::wstring& name, int size)
{
    FontHandle handle;
    handle.size = size;

    if (!m_DWrite)
        return handle;

    // Resources/Font/<name>.ttf 를 DirectWrite 폰트페이스로 직접 로드
    std::wstring path = L"Resources/Font/" + name + L".ttf";

    ComPtr<IDWriteFontFile> file;
    if (FAILED(m_DWrite->CreateFontFileReference(path.c_str(), nullptr, &file)))
        return handle;

    BOOL isSupported = FALSE;
    DWRITE_FONT_FILE_TYPE fileType;
    DWRITE_FONT_FACE_TYPE faceType;
    UINT32 numFaces = 0;
    file->Analyze(&isSupported, &fileType, &faceType, &numFaces);
    if (!isSupported)
        return handle;

    auto* atlas = new FontAtlas();
    IDWriteFontFile* files[] = { file.Get() };
    m_DWrite->CreateFontFace(faceType, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &atlas->face);

    atlas->emSize = (float)size;

    DWRITE_FONT_METRICS fm{};
    atlas->face->GetMetrics(&fm);
    float scale = atlas->emSize / (float)fm.designUnitsPerEm;
    atlas->ascent = fm.ascent * scale;

    // 아틀라스 텍스처(RGBA8, DEFAULT + UpdateSubresource 로 글리프 업로드)
    D3D11_TEXTURE2D_DESC td{};
    td.Width = kAtlasSize;
    td.Height = kAtlasSize;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    m_Device->CreateTexture2D(&td, nullptr, &atlas->tex);
    m_Device->CreateShaderResourceView(atlas->tex.Get(), nullptr, &atlas->srv);

    handle.font = atlas;
    return handle;
}

void DX11Renderer::ReleaseFont(FontHandle& handle)
{
    if (handle.font)
    {
        delete static_cast<FontAtlas*>(handle.font);
        handle.font = nullptr;
    }
}

void DX11Renderer::DrawTextImpl(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color)
{
    if (!handle || !handle->font || !m_DWrite)
        return;

    auto* atlas = static_cast<FontAtlas*>(handle->font);
    if (!atlas->face)
        return;

    DWRITE_FONT_METRICS fm{};
    atlas->face->GetMetrics(&fm);
    float scale = atlas->emSize / (float)fm.designUnitsPerEm;

    float penX = (float)x;
    float baseline = (float)y + atlas->ascent; // 텍스트 상단 y 에 맞춤(DXTK SpriteFont 와 동일)

    XMFLOAT4 col{ color.r, color.g, color.b, color.a };

    for (wchar_t ch : text)
    {
        // 캐시에 없으면 이 순간 래스터라이즈해서 아틀라스에 굽는다 (CJK 대응 핵심)
        auto it = atlas->glyphs.find(ch);
        if (it == atlas->glyphs.end())
        {
            GlyphInfo gi{};

            UINT32 codepoint = (UINT32)ch;
            UINT16 glyphIndex = 0;
            atlas->face->GetGlyphIndices(&codepoint, 1, &glyphIndex);

            DWRITE_GLYPH_METRICS gm{};
            atlas->face->GetDesignGlyphMetrics(&glyphIndex, 1, &gm, FALSE);
            gi.advance = gm.advanceWidth * scale;

            float zeroAdvance = 0.f;
            DWRITE_GLYPH_OFFSET zeroOffset{ 0.f, 0.f };
            DWRITE_GLYPH_RUN run{};
            run.fontFace = atlas->face.Get();
            run.fontEmSize = atlas->emSize;
            run.glyphCount = 1;
            run.glyphIndices = &glyphIndex;
            run.glyphAdvances = &zeroAdvance;
            run.glyphOffsets = &zeroOffset;

            ComPtr<IDWriteGlyphRunAnalysis> analysis;
            HRESULT hr = m_DWrite->CreateGlyphRunAnalysis(
                &run, 1.0f, nullptr,
                DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL,
                0.0f, 0.0f, &analysis);

            RECT b{ 0, 0, 0, 0 };
            if (SUCCEEDED(hr))
                analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &b);

            int gw = b.right - b.left;
            int gh = b.bottom - b.top;

            if (gw > 0 && gh > 0)
            {
                // ClearType 3바이트(RGB) 커버리지를 받아 그레이스케일로 평균 → RGBA 아틀라스
                std::vector<BYTE> ct((size_t)gw * gh * 3);
                analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &b, ct.data(), (UINT32)ct.size());

                std::vector<BYTE> rgba((size_t)gw * gh * 4);
                for (int i = 0; i < gw * gh; ++i)
                {
                    BYTE cov = (BYTE)(((int)ct[i * 3] + ct[i * 3 + 1] + ct[i * 3 + 2]) / 3);
                    rgba[i * 4 + 0] = 255;
                    rgba[i * 4 + 1] = 255;
                    rgba[i * 4 + 2] = 255;
                    rgba[i * 4 + 3] = cov;
                }

                // 셸프 패킹: 현재 행에 안 들어가면 다음 행으로
                if (atlas->penX + gw + 1 > (int)kAtlasSize)
                {
                    atlas->penX = 1;
                    atlas->penY += atlas->rowHeight + 1;
                    atlas->rowHeight = 0;
                }
                if (gh > atlas->rowHeight) atlas->rowHeight = gh;

                int dstX = atlas->penX;
                int dstY = atlas->penY;

                D3D11_BOX box{};
                box.left = dstX; box.top = dstY; box.front = 0;
                box.right = dstX + gw; box.bottom = dstY + gh; box.back = 1;
                m_Context->UpdateSubresource(atlas->tex.Get(), 0, &box, rgba.data(), gw * 4, 0);

                gi.u0 = (float)dstX / kAtlasSize;
                gi.v0 = (float)dstY / kAtlasSize;
                gi.u1 = (float)(dstX + gw) / kAtlasSize;
                gi.v1 = (float)(dstY + gh) / kAtlasSize;
                gi.width = gw;
                gi.height = gh;
                gi.offsetX = b.left;
                gi.offsetY = b.top; // baseline 기준(위쪽 음수)

                atlas->penX += gw + 1;
            }

            it = atlas->glyphs.emplace(ch, gi).first;
        }

        const GlyphInfo& g = it->second;

        if (g.width > 0 && g.height > 0)
        {
            float gx = penX + g.offsetX;
            float gy = baseline + g.offsetY;
            float gw = (float)g.width;
            float gh = (float)g.height;

            SpriteVertex quad[6] =
            {
                { { gx,      gy      }, { g.u0, g.v0 }, col },
                { { gx + gw, gy      }, { g.u1, g.v0 }, col },
                { { gx + gw, gy + gh }, { g.u1, g.v1 }, col },
                { { gx,      gy      }, { g.u0, g.v0 }, col },
                { { gx + gw, gy + gh }, { g.u1, g.v1 }, col },
                { { gx,      gy + gh }, { g.u0, g.v1 }, col },
            };
            PushQuad(atlas->srv.Get(), quad);
        }

        penX += g.advance;
    }
}

void DX11Renderer::DrawText(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color)
{
    DrawTextImpl(handle, text, x, y, color);
}

void DX11Renderer::DrawText(FontHandle* handle, const std::string& text, int x, int y, const Color& color)
{
    std::wstring w(text.begin(), text.end());
    DrawTextImpl(handle, w, x, y, color);
}

// ---- 텍스처 (WIC 직접 로드) ---------------------------------------------

TextureHandle DX11Renderer::CreateTexture(const std::wstring& path)
{
    TextureHandle handle;

    if (!m_WIC)
        return handle;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(m_WIC->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder)))
        return handle;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)))
        return handle;

    // 어떤 포맷이든 32bpp RGBA 로 변환 (DXGI_FORMAT_R8G8B8A8_UNORM 대응)
    ComPtr<IWICFormatConverter> converter;
    m_WIC->CreateFormatConverter(&converter);
    converter->Initialize(
        frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);

    std::vector<BYTE> pixels((size_t)w * h * 4);
    converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = w * 4;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_Device->CreateTexture2D(&td, &init, &tex)))
        return handle;

    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(m_Device->CreateShaderResourceView(tex.Get(), nullptr, &srv)))
        return handle;

    handle.texture = srv.Detach(); // TextureHandle 이 SRV 소유(ReleaseTexture 에서 해제)
    handle.width = (int)w;
    handle.height = (int)h;
    return handle;
}

void DX11Renderer::ReleaseTexture(TextureHandle& handle)
{
    if (handle.texture)
    {
        static_cast<ID3D11ShaderResourceView*>(handle.texture)->Release();
        handle.texture = nullptr;
        handle.width = 0;
        handle.height = 0;
    }
}
