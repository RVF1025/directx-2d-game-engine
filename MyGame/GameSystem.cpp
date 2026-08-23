#include "GameSystem.h"
#include "GraphicManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "Timer.h"
#include "Window.h"

#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

GameSystem::GameSystem()
{
}


GameSystem::~GameSystem()
{
}

void GameSystem::Init()
{
	Window window;
	window.Create(L"Bit Bit 8-Bit Jump!", SCREEN_WIDTH, SCREEN_HEIGHT);
	window.Show();

	GraphicManager::RenderAPI renderer;
	fs::path iniPath = fs::current_path() / "Data" / "Config" / "config.ini";

	switch (GetPrivateProfileIntW(L"Graphics", L"Renderer", 9, iniPath.c_str()))
	{
	case 9:
		renderer = GraphicManager::RenderAPI::DX9;
		break;
	case 11:
		renderer = GraphicManager::RenderAPI::DX11;
		break;
	case 12:
		renderer = GraphicManager::RenderAPI::DX11Instanced;
		break;
	default:
		renderer = GraphicManager::RenderAPI::DX9;
		break;
	}

	GraphicManager::GetInstance()->Init(renderer, window.GetHWnd());
	TextureManager::GetInstance()->Init();
	InputManager::GetInstance()->Init(window.GetHWnd(), SCREEN_WIDTH, SCREEN_HEIGHT);
	// [Stress] Count > 0 이면 벤치마크용 스트레스 씬으로 진입(스프라이트 대량 렌더).
	int stressCount = GetPrivateProfileIntW(L"Stress", L"Count", 0, iniPath.c_str());
	SceneManager::GetInstance()->SceneChange(
		stressCount > 0 ? SceneManager::eStress : SceneManager::eStage1);

	// [Graphics] ShowFPS = 1 이면 창 타이틀에 초당 프레임 수 표시(벤치마크/디버그용). 기본 0.
	bool showFPS = GetPrivateProfileIntW(L"Graphics", L"ShowFPS", 0, iniPath.c_str()) != 0;

	// 스트레스 모드 벤치마크 자동 기록용 컨텍스트
	bool  benchMode   = stressCount > 0;
	int   rendererId  = GetPrivateProfileIntW(L"Graphics", L"Renderer", 9, iniPath.c_str());
	int   sTextures   = GetPrivateProfileIntW(L"Stress", L"Textures", 1, iniPath.c_str());
	int   sAtlas      = GetPrivateProfileIntW(L"Stress", L"Atlas", 0, iniPath.c_str());
	int   sAtlasPages = GetPrivateProfileIntW(L"Stress", L"AtlasPages", 1, iniPath.c_str());
	int   sSort       = GetPrivateProfileIntW(L"Stress", L"Sort", 0, iniPath.c_str());

	// 워밍업 1초 후 5초 동안 평균 FPS 측정 → bench_results.csv 에 한 줄 기록(런당 1회)
	const float kWarmup = 1.0f, kWindow = 10.0f;
	float benchWarm = 0.f, benchElapsed = 0.f;
	int   benchFrames = 0;
	bool  benchLogged = false;

	int fps = 0;
	float acc = 0.f;
	std::string name = "FPS : 0";

	Timer timer;
	while (window.MsgLoop())
	{
		float dt = (float)timer.Update();

		SceneManager::GetInstance()->m_Scene->Update(dt);
		SoundManager::GetInstance()->Update();

		GraphicManager::GetInstance()->RenderStart();
		SceneManager::GetInstance()->m_Scene->Render();

		if (showFPS && !benchLogged)
		{
			fps++;
			acc += dt;
			if (acc >= 1.f)
			{
				name = "FPS : " + std::to_string(fps);
				SetWindowTextA(window.GetHWnd(), name.c_str());
				acc -= 1.f;
				fps = 0;
			}
		}

		// 스트레스 모드: 평균 FPS 측정 후 CSV 기록
		if (benchMode && !benchLogged)
		{
			benchWarm += dt;
			if (benchWarm >= kWarmup)
			{
				benchElapsed += dt;
				benchFrames++;
				if (benchElapsed >= kWindow)
				{
					int avg = (int)(benchFrames / benchElapsed);

					fs::path csv = fs::current_path() / "bench_results.csv";
					bool existed = fs::exists(csv);
					std::ofstream ofs(csv, std::ios::app);
					if (ofs)
					{
						if (!existed)
							ofs << "Renderer,Count,Textures,Atlas,AtlasPages,Sort,AvgFPS\n";
						ofs << rendererId << "," << stressCount << "," << sTextures << ","
							<< sAtlas << "," << sAtlasPages << "," << sSort << "," << avg << "\n";
					}

					name = "AVG FPS : " + std::to_string(avg);   // 화면 HUD: 깔끔하게
					SetWindowTextA(window.GetHWnd(),
						(name + "  (logged to bench_results.csv)").c_str()); // 타이틀바: 안내 포함
					benchLogged = true;
				}
			}
		}

		// 화면 내 FPS HUD — 타이틀바를 못 잡는 캡처 툴(게임바 등)에서도 보이도록 클라이언트 영역에 직접 렌더
		// 흰색 배경 박스 + 검정 큰 글씨로 어떤 배경에서도 잘 보이게
		if (showFPS)
		{
			IRenderer* r = GraphicManager::GetInstance()->GetRenderer();
			const int tx = 18, ty = 12;                     // 텍스트 위치
			float boxW = name.size() * 26.f + 24.f;          // 글자 수 기반 박스 폭
			float boxH = 64.f;
			float boxCx = 8.f + boxW * 0.5f;                 // DrawSprite 는 중심 원점
			float boxCy = 6.f + boxH * 0.5f;

			TextureHandle* white = TextureManager::GetInstance()->GetTexture("white");
			if (white && white->width > 0)
			{
				r->SpriteRenderStart();
				r->DrawSprite(white, boxCx, boxCy, boxW / white->width, boxH / white->height, 0.f, false);
				r->SpriteRenderEnd();
			}

			Color black{ 0.f, 0.f, 0.f, 1.f };
			r->FontRenderStart();
			r->DrawText(TextureManager::GetInstance()->GetLargeFont(), name, tx, ty, black);
			r->FontRenderEnd();
		}

		GraphicManager::GetInstance()->RenderEnd();
	}
}

void GameSystem::Release()
{
	SceneManager::GetInstance()->m_Scene->Release();
	TextureManager::GetInstance()->Release();
	GraphicManager::GetInstance()->Release();
	Manager::GetInstance()->ReleaseAll();
}