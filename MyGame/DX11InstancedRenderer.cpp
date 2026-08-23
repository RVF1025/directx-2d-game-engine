#include "DX11InstancedRenderer.h"

#include <d3dcompiler.h>
#include <dwrite.h>
#include <wincodec.h>
#include <objbase.h>
#include <dxgi1_5.h>

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
    // 공유 쿼드 정점(corner)에 인스턴스별 데이터(center/size/rot/uv/color)를 적용해 변형.
    // 정점 변형이 전부 VS(GPU)에서 일어난다 — CPU 는 인스턴스 레코드만 채운다.
    const char* kShaderSrc = R"(
cbuffer CB : register(b0)
{
    row_major float4x4 gProj;
};

struct VSIn
{
    float2 corner  : POSITION;      // 공유 쿼드 모서리 (-0.5~0.5)
    float2 cuv     : TEXCOORD;      // 공유 쿼드 UV 가중치 (0~1)
    float2 icenter : INST_CENTER;   // 이하 인스턴스별
    float2 isize   : INST_SIZE;
    float  irot    : INST_ROT;
    float2 iuvmin  : INST_UVMIN;
    float2 iuvmax  : INST_UVMAX;
    float4 icolor  : INST_COLOR;
};

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    float2 p = i.corner * i.isize;               // 스케일
    float s = sin(i.irot), c = cos(i.irot);
    float2 rp = float2(p.x * c - p.y * s, p.x * s + p.y * c); // 회전
    float2 world = i.icenter + rp;               // 이동
    o.pos = mul(float4(world, 0.0f, 1.0f), gProj);
    o.uv  = lerp(i.iuvmin, i.iuvmax, i.cuv);     // 모서리 가중치로 UV 보간
    o.col = i.icolor;
    return o;
}

Texture2D    gTex : register(t0);
SamplerState gSmp : register(s0);

float4 PSMain(VSOut i) : SV_Target
{
    return gTex.Sample(gSmp, i.uv) * i.col;
}
)";

    constexpr UINT kMaxInstances = 240000; // 인스턴스 링 버퍼 용량 (52B/인스턴스 ≈ 12.5MB)
    constexpr UINT kAtlasSize = 1024;

    std::wstring ConfigPath()
    {
        wchar_t dir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, dir);
        return std::wstring(dir) + L"\\Data\\Config\\config.ini";
    }
    bool ReadVSyncConfig()  { return GetPrivateProfileIntW(L"Graphics", L"VSync", 1, ConfigPath().c_str()) != 0; }
    bool ReadSortConfig()   { return GetPrivateProfileIntW(L"Stress",  L"Sort",  0, ConfigPath().c_str()) != 0; }
}

// 글리프 아틀라스 (DX11Renderer 와 동일 방식)
struct GlyphInfoI
{
    float u0, v0, u1, v1;
    int   width, height;
    int   offsetX, offsetY;
    float advance;
};

struct FontAtlasI
{
    ComPtr<IDWriteFontFace>           face;
    ComPtr<ID3D11Texture2D>           tex;
    ComPtr<ID3D11ShaderResourceView>  srv;
    float emSize = 0.f;
    float ascent = 0.f;
    int penX = 1, penY = 1, rowHeight = 0;
    std::unordered_map<wchar_t, GlyphInfoI> glyphs;
};

DX11InstancedRenderer::DX11InstancedRenderer() {}
DX11InstancedRenderer::~DX11InstancedRenderer() {}

void DX11InstancedRenderer::Init(HWND hWnd)
{
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_ComInitialized = SUCCEEDED(hrCo);

    m_VSync = ReadVSyncConfig();
    m_SortByTexture = ReadSortConfig();

    CreateDevice(hWnd);
    CreatePipeline();
    BindPipeline();

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_DWrite.GetAddressOf()));

    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_WIC.GetAddressOf()));

    m_Batch.reserve(kMaxInstances);
}

void DX11InstancedRenderer::Release()
{
    m_Batch.clear();
    if (m_Context) m_Context->ClearState();
    m_WIC.Reset();
    m_DWrite.Reset();
    if (m_ComInitialized)
    {
        CoUninitialize();
        m_ComInitialized = false;
    }
}

void DX11InstancedRenderer::CreateDevice(HWND hWnd)
{
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
    sd.BufferCount = 2;
    sd.BufferDesc.Width = SCREEN_WIDTH;
    sd.BufferDesc.Height = SCREEN_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
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
    vp.MaxDepth = 1.0f;
    m_Context->RSSetViewports(1, &vp);
}

void DX11InstancedRenderer::CreatePipeline()
{
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

    // 슬롯 0 = 정점당(공유 쿼드), 슬롯 1 = 인스턴스당(StepRate 1)
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",    0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "INST_CENTER", 0, DXGI_FORMAT_R32G32_FLOAT,       1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1,  8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_UVMIN",  0, DXGI_FORMAT_R32G32_FLOAT,       1, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_UVMAX",  0, DXGI_FORMAT_R32G32_FLOAT,       1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 36, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    m_Device->CreateInputLayout(layout, _countof(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_InputLayout);

    // 공유 쿼드 정점/인덱스 (정적, IMMUTABLE)
    QuadVertex quad[4] =
    {
        { { -0.5f, -0.5f }, { 0.f, 0.f } },
        { {  0.5f, -0.5f }, { 1.f, 0.f } },
        { {  0.5f,  0.5f }, { 1.f, 1.f } },
        { { -0.5f,  0.5f }, { 0.f, 1.f } },
    };
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC qvd{};
    qvd.ByteWidth = sizeof(quad);
    qvd.Usage = D3D11_USAGE_IMMUTABLE;
    qvd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA qvi{ quad, 0, 0 };
    m_Device->CreateBuffer(&qvd, &qvi, &m_QuadVB);

    D3D11_BUFFER_DESC qid{};
    qid.ByteWidth = sizeof(idx);
    qid.Usage = D3D11_USAGE_IMMUTABLE;
    qid.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA qii{ idx, 0, 0 };
    m_Device->CreateBuffer(&qid, &qii, &m_QuadIB);

    // 인스턴스 동적 링 버퍼
    D3D11_BUFFER_DESC ivd{};
    ivd.ByteWidth = sizeof(SpriteInstance) * kMaxInstances;
    ivd.Usage = D3D11_USAGE_DYNAMIC;
    ivd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ivd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_Device->CreateBuffer(&ivd, nullptr, &m_InstanceVB);

    // 상수 버퍼(직교투영)
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

    // 블렌드/샘플러/래스터 (배칭 렌더러와 동일)
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

    D3D11_SAMPLER_DESC smp{};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    m_Device->CreateSamplerState(&smp, &m_SamplerState);

    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = TRUE;
    m_Device->CreateRasterizerState(&rs, &m_RasterState);
}

void DX11InstancedRenderer::BindPipeline()
{
    ID3D11Buffer* vbs[2] = { m_QuadVB.Get(), m_InstanceVB.Get() };
    UINT strides[2] = { sizeof(QuadVertex), sizeof(SpriteInstance) };
    UINT offsets[2] = { 0, 0 };

    m_Context->IASetInputLayout(m_InputLayout.Get());
    m_Context->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    m_Context->IASetIndexBuffer(m_QuadIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_Context->VSSetShader(m_VS.Get(), nullptr, 0);
    m_Context->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
    m_Context->PSSetShader(m_PS.Get(), nullptr, 0);
    m_Context->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

    float blendFactor[4] = { 0, 0, 0, 0 };
    m_Context->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xffffffff);
    m_Context->RSSetState(m_RasterState.Get());
}

void DX11InstancedRenderer::RenderStart()
{
    float clear[4] = { 0.f, 0.f, 0.f, 1.f };
    m_Context->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);
    m_Context->ClearRenderTargetView(m_RTV.Get(), clear);

    m_InstUsed = 0;
    m_CurSRV = nullptr;
    m_Batch.clear();
    m_Buckets.clear();
}

void DX11InstancedRenderer::RenderEnd()
{
    FlushBatch();
    if (m_VSync)
        m_SwapChain->Present(1, 0);
    else
    {
        UINT presentFlags = m_AllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        m_SwapChain->Present(0, presentFlags);
    }
}

void DX11InstancedRenderer::SpriteRenderStart() {}
void DX11InstancedRenderer::SpriteRenderEnd()   { FlushBatch(); }
void DX11InstancedRenderer::FontRenderStart()   {}
void DX11InstancedRenderer::FontRenderEnd()     { FlushBatch(); }

// ---- 인스턴스 배치 -------------------------------------------------------

// 인스턴스 배열을 링 버퍼에 올려 DrawIndexedInstanced 1회
void DX11InstancedRenderer::DrawInstances(ID3D11ShaderResourceView* srv, const SpriteInstance* data, UINT count)
{
    if (!srv || count == 0)
        return;

    if (m_InstUsed + count > kMaxInstances)
        m_InstUsed = 0;

    D3D11_MAP mapType = (m_InstUsed == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    m_Context->Map(m_InstanceVB.Get(), 0, mapType, 0, &mapped);
    memcpy(reinterpret_cast<SpriteInstance*>(mapped.pData) + m_InstUsed,
        data, count * sizeof(SpriteInstance));
    m_Context->Unmap(m_InstanceVB.Get(), 0);

    m_Context->PSSetShaderResources(0, 1, &srv);
    m_Context->DrawIndexedInstanced(6, count, 0, 0, m_InstUsed); // StartInstanceLocation = 링 오프셋
    m_InstUsed += count;
}

void DX11InstancedRenderer::PushInstance(ID3D11ShaderResourceView* srv, const SpriteInstance& inst)
{
    if (m_SortByTexture)
    {
        // 정렬 모드: SRV별 버킷에 모아두고 *RenderEnd 에서 한꺼번에 그린다(순서 재정렬).
        m_Buckets[srv].push_back(inst);
        return;
    }

    // 비정렬 모드: 연속 같은 SRV만 병합, 바뀌면 즉시 flush(제출 순서 보존).
    if (srv != m_CurSRV)
    {
        FlushBatch();
        m_CurSRV = srv;
    }
    if (m_Batch.size() + 1 > kMaxInstances)
        FlushBatch();
    m_Batch.push_back(inst);
}

void DX11InstancedRenderer::FlushBatch()
{
    if (m_SortByTexture)
    {
        // SRV당 draw 1회. 같은 아틀라스가 프레임 내 흩어져 있어도 전부 한 번에 병합됨.
        for (auto& kv : m_Buckets)
        {
            const std::vector<SpriteInstance>& v = kv.second;
            UINT off = 0;
            while (off < v.size())
            {
                UINT n = (UINT)v.size() - off;
                if (n > kMaxInstances) n = kMaxInstances;
                DrawInstances(kv.first, v.data() + off, n);
                off += n;
            }
        }
        m_Buckets.clear();
        return;
    }

    if (m_Batch.empty() || m_CurSRV == nullptr)
    {
        m_Batch.clear();
        return;
    }
    DrawInstances(m_CurSRV, m_Batch.data(), (UINT)m_Batch.size());
    m_Batch.clear();
}

// ---- 스프라이트 ----------------------------------------------------------

void DX11InstancedRenderer::DrawSprite(TextureHandle* handle, float x, float y, float sx, float sy, float rot, bool flip)
{
    if (!handle || !handle->texture)
        return;

    auto srv = static_cast<ID3D11ShaderResourceView*>(handle->texture);

    SpriteInstance inst;
    inst.center = { x, y };
    inst.size = { handle->width * sx, handle->height * sy };
    inst.rot = XMConvertToRadians(rot);
    // 아틀라스 서브렉트 UV(기본 0..1). flip 이면 u 뒤집기.
    inst.uvMin = { flip ? handle->u1 : handle->u0, handle->v0 };
    inst.uvMax = { flip ? handle->u0 : handle->u1, handle->v1 };
    inst.color = { 1.f, 1.f, 1.f, 1.f };

    PushInstance(srv, inst);
}

// ---- 폰트 (배칭 렌더러와 동일한 온디맨드 글리프 아틀라스) ------------------

FontHandle DX11InstancedRenderer::CreateFont(const std::wstring& name, int size)
{
    FontHandle handle;
    handle.size = size;
    if (!m_DWrite)
        return handle;

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

    auto* atlas = new FontAtlasI();
    IDWriteFontFile* files[] = { file.Get() };
    m_DWrite->CreateFontFace(faceType, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &atlas->face);

    atlas->emSize = (float)size;
    DWRITE_FONT_METRICS fm{};
    atlas->face->GetMetrics(&fm);
    atlas->ascent = fm.ascent * (atlas->emSize / (float)fm.designUnitsPerEm);

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

void DX11InstancedRenderer::ReleaseFont(FontHandle& handle)
{
    if (handle.font)
    {
        delete static_cast<FontAtlasI*>(handle.font);
        handle.font = nullptr;
    }
}

void DX11InstancedRenderer::DrawTextImpl(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color)
{
    if (!handle || !handle->font || !m_DWrite)
        return;

    auto* atlas = static_cast<FontAtlasI*>(handle->font);
    if (!atlas->face)
        return;

    DWRITE_FONT_METRICS fm{};
    atlas->face->GetMetrics(&fm);
    float scale = atlas->emSize / (float)fm.designUnitsPerEm;

    float penX = (float)x;
    float baseline = (float)y + atlas->ascent;
    XMFLOAT4 col{ color.r, color.g, color.b, color.a };

    for (wchar_t ch : text)
    {
        auto it = atlas->glyphs.find(ch);
        if (it == atlas->glyphs.end())
        {
            GlyphInfoI gi{};
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
                std::vector<BYTE> ct((size_t)gw * gh * 3);
                analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &b, ct.data(), (UINT32)ct.size());
                std::vector<BYTE> rgba((size_t)gw * gh * 4);
                for (int i = 0; i < gw * gh; ++i)
                {
                    BYTE cov = (BYTE)(((int)ct[i * 3] + ct[i * 3 + 1] + ct[i * 3 + 2]) / 3);
                    rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255; rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = cov;
                }

                if (atlas->penX + gw + 1 > (int)kAtlasSize)
                {
                    atlas->penX = 1;
                    atlas->penY += atlas->rowHeight + 1;
                    atlas->rowHeight = 0;
                }
                if (gh > atlas->rowHeight) atlas->rowHeight = gh;

                int dstX = atlas->penX, dstY = atlas->penY;
                D3D11_BOX box{};
                box.left = dstX; box.top = dstY; box.front = 0;
                box.right = dstX + gw; box.bottom = dstY + gh; box.back = 1;
                m_Context->UpdateSubresource(atlas->tex.Get(), 0, &box, rgba.data(), gw * 4, 0);

                gi.u0 = (float)dstX / kAtlasSize;
                gi.v0 = (float)dstY / kAtlasSize;
                gi.u1 = (float)(dstX + gw) / kAtlasSize;
                gi.v1 = (float)(dstY + gh) / kAtlasSize;
                gi.width = gw; gi.height = gh;
                gi.offsetX = b.left; gi.offsetY = b.top;
                atlas->penX += gw + 1;
            }
            it = atlas->glyphs.emplace(ch, gi).first;
        }

        const GlyphInfoI& g = it->second;
        if (g.width > 0 && g.height > 0)
        {
            float gx = penX + g.offsetX;
            float gy = baseline + g.offsetY;

            SpriteInstance inst;
            inst.center = { gx + g.width * 0.5f, gy + g.height * 0.5f };
            inst.size = { (float)g.width, (float)g.height };
            inst.rot = 0.f;
            inst.uvMin = { g.u0, g.v0 };
            inst.uvMax = { g.u1, g.v1 };
            inst.color = col;
            PushInstance(atlas->srv.Get(), inst);
        }
        penX += g.advance;
    }
}

void DX11InstancedRenderer::DrawText(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color)
{
    DrawTextImpl(handle, text, x, y, color);
}

void DX11InstancedRenderer::DrawText(FontHandle* handle, const std::string& text, int x, int y, const Color& color)
{
    std::wstring w(text.begin(), text.end());
    DrawTextImpl(handle, w, x, y, color);
}

// ---- 텍스처 (WIC 직접 로드, 배칭 렌더러와 동일) --------------------------

TextureHandle DX11InstancedRenderer::CreateTexture(const std::wstring& path)
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
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
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

    handle.texture = srv.Detach();
    handle.width = (int)w;
    handle.height = (int)h;
    return handle;
}

void DX11InstancedRenderer::ReleaseTexture(TextureHandle& handle)
{
    if (handle.texture)
    {
        static_cast<ID3D11ShaderResourceView*>(handle.texture)->Release();
        handle.texture = nullptr;
        handle.width = 0;
        handle.height = 0;
    }
}
