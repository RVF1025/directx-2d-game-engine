#pragma once
#include "Definitions.h"
#include "InputManager.h"
#include "IRenderer.h"
// 구체 렌더러 헤더(DX9Renderer/DX11Renderer)는 .cpp 에서만 include.
// → d3dx9(구형 DX SDK) / d3d11 헤더가 이 헤더를 include 하는 다른 TU 로 새지 않게 하여
//   컴파일 결합도와 헤더 오염(예: <filesystem> 충돌)을 차단.

class GraphicManager : public Singleton<GraphicManager>
{
public:
	enum class RenderAPI
	{
		DX9,
		DX11,
		DX11Instanced
	};

	GraphicManager();
	~GraphicManager();

	void Init(RenderAPI api, HWND hWnd);
	void Release();

	void RenderStart();
	void RenderEnd();

	IRenderer* GetRenderer() { return m_Renderer.get(); }

private:
	std::unique_ptr<IRenderer> m_Renderer;
};