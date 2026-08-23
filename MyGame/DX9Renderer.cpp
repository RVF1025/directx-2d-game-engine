#include "DX9Renderer.h"

namespace
{
    // config.ini [Graphics] VSync 읽기 (기본 1=켜짐). DX11 백엔드와 동일 규칙으로 읽어
    // 두 백엔드를 같은 조건에서 벤치마크할 수 있게 한다.
    bool ReadVSyncConfig()
    {
        wchar_t dir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, dir);
        std::wstring path = std::wstring(dir) + L"\\Data\\Config\\config.ini";
        return GetPrivateProfileIntW(L"Graphics", L"VSync", 1, path.c_str()) != 0;
    }
}

DX9Renderer::DX9Renderer() {}
DX9Renderer::~DX9Renderer() {}

void DX9Renderer::Init(HWND hWnd)
{
	m_D3D = Direct3DCreate9(D3D_SDK_VERSION);

	D3DCAPS9			DeviceCaps;
	ZeroMemory(&DeviceCaps, sizeof(D3DCAPS9));

	m_D3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &DeviceCaps);

	unsigned long		uFlag = 0;

	if (DeviceCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		uFlag |= D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED;

	else
		uFlag |= D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED;

	D3DDISPLAYMODE d3ddm;
	m_D3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

	D3DPRESENT_PARAMETERS			Present_Parameter;
	ZeroMemory(&Present_Parameter, sizeof(D3DPRESENT_PARAMETERS));

	Present_Parameter.BackBufferWidth = SCREEN_WIDTH;
	Present_Parameter.BackBufferHeight = SCREEN_HEIGHT;
	Present_Parameter.BackBufferFormat = D3DFMT_A8R8G8B8;
	Present_Parameter.BackBufferCount = 1;

	Present_Parameter.MultiSampleType = D3DMULTISAMPLE_NONE;
	Present_Parameter.MultiSampleQuality = 0;

	Present_Parameter.SwapEffect = D3DSWAPEFFECT_DISCARD;
	Present_Parameter.hDeviceWindow = hWnd;

	Present_Parameter.Windowed = true;
	Present_Parameter.EnableAutoDepthStencil = TRUE;
	Present_Parameter.AutoDepthStencilFormat = D3DFMT_D16;
	//D3DFMT_D24S8
	Present_Parameter.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	// VSync: INTERVAL_ONE = 주사율에 동기(제한), IMMEDIATE = 제한 해제. config.ini 로 선택.
	Present_Parameter.PresentationInterval = ReadVSyncConfig()
		? D3DPRESENT_INTERVAL_ONE
		: D3DPRESENT_INTERVAL_IMMEDIATE;

	m_D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		uFlag,
		&Present_Parameter, &m_Device);

	D3DXCreateSprite(m_Device, &m_Sprite);
}

void DX9Renderer::Release()
{
	if (m_Sprite != NULL)
		m_Sprite->Release();

	if (m_Device != NULL)
		m_Device->Release();

	if (m_D3D != NULL)
		m_D3D->Release();
}

void DX9Renderer::RenderStart()
{
	m_Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
	m_Device->BeginScene();
}

void DX9Renderer::RenderEnd()
{
	m_Device->EndScene();
	m_Device->Present(NULL, NULL, NULL, NULL);
}

void DX9Renderer::SpriteRenderStart()
{
	m_Sprite->Begin(D3DXSPRITE_ALPHABLEND);
}

void DX9Renderer::SpriteRenderEnd()
{
	m_Sprite->End();
}

void DX9Renderer::FontRenderStart()
{
}

void DX9Renderer::FontRenderEnd()
{
}

TextureHandle DX9Renderer::CreateTexture(const std::wstring& path)
{
	TextureHandle handle;

	LPDIRECT3DTEXTURE9 tex;
	D3DXIMAGE_INFO info;
	D3DXCreateTextureFromFileExW(
		m_Device,
		path.c_str(),
		D3DX_DEFAULT_NONPOW2,
		D3DX_DEFAULT_NONPOW2,
		1,
		0,
		D3DFMT_UNKNOWN,
		D3DPOOL_MANAGED,
		1,
		1,
		NULL,
		&info,
		NULL,
		&tex);

	handle.texture = tex;
	handle.width = info.Width;
	handle.height = info.Height;

	return handle;
}

void DX9Renderer::ReleaseTexture(TextureHandle& handle)
{
	if (handle.texture)
	{
		auto texture = static_cast<LPDIRECT3DTEXTURE9>(handle.texture);
		if (!texture)
			return;

		texture->Release();
		handle.texture = nullptr;
	}
}

FontHandle DX9Renderer::CreateFont(const std::wstring& name, int size)
{
	FontHandle handle;
	handle.size = size;

	LPD3DXFONT font = nullptr;

	D3DXCreateFontW(
		m_Device,
		size,
		0,
		FW_NORMAL,
		1,
		FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		name.c_str(),
		&font
	);

	handle.font = font;
	return handle;
}

void DX9Renderer::ReleaseFont(FontHandle& handle)
{
	if (handle.font)
	{
		auto font = static_cast<LPD3DXFONT>(handle.font);
		if (!font)
			return;

		font->Release();
		handle.font = nullptr;
	}
}

void DX9Renderer::DrawSprite(TextureHandle* handle, float x, float y, float sx, float sy, float rot, bool flip)
{
	if (!handle || !handle->texture)
		return;

	auto texture = reinterpret_cast<LPDIRECT3DTEXTURE9>(handle->texture);
	if (!texture)
		return;

	D3DXMATRIX mat;
	D3DXMATRIX Trns;
	D3DXMATRIX Rot;
	D3DXMATRIX Scale;

	D3DXMatrixIdentity(&mat);
	D3DXMatrixIdentity(&Trns);
	D3DXMatrixIdentity(&Rot);
	D3DXMatrixIdentity(&Scale);

	D3DXMatrixScaling(&Scale, flip ? -sx : sx, sy, 1.0f);
	D3DXMatrixRotationZ(&Rot, D3DXToRadian(rot));
	D3DXMatrixTranslation(&Trns, x, y, 0);

	mat = Scale * Rot * Trns;

	D3DXVECTOR3 center(handle->width * 0.5f, handle->height * 0.5f, 0.f);

	// 아틀라스 서브렉트: 소스 픽셀 영역 지정(기본 핸들은 전체 = {0,0,width,height}).
	RECT src;
	SetRect(&src, handle->srcX, handle->srcY,
		handle->srcX + handle->width, handle->srcY + handle->height);

	m_Sprite->SetTransform(&mat);
	m_Sprite->Draw(texture, &src, &center, nullptr, 0xffffffff);
}

void DX9Renderer::DrawText(FontHandle* handle, const std::wstring& text, int x, int y, const Color& color)
{
	if (!handle || !handle->font)
		return;

	auto font = static_cast<LPD3DXFONT>(handle->font);
	if (!font)
		return;

	RECT rect;
	SetRect(&rect, x, y, 0, 0);

	D3DXCOLOR c(color.r, color.g, color.b, color.a);
	font->DrawTextW(NULL, text.c_str(), -1, &rect, DT_NOCLIP, c);
}

void DX9Renderer::DrawText(FontHandle* handle, const std::string& text, int x, int y, const Color& color)
{
	std::wstring w(text.begin(), text.end());
	DrawText(handle, w, x, y, color);
}