#pragma once
#include "Scene.h"
#include "MyECS.h"
#include "Systems.h"
#include "Components.h"

#include <cstdlib>
#include <string>

// 벤치마크용 스트레스 씬.
// 플레이어/로직 없이 SpriteDrawSystem 만 등록 → 카메라 원점 고정, 컬링 없이 스프라이트를 전량 렌더.
// config.ini [Stress] Count = 스프라이트 수, Textures = 사용할 서로 다른 텍스처 종류 수(드로우콜 부하 조절).
class StressStage : public Scene
{
public:
	virtual void Init() override
	{
		wchar_t dir[MAX_PATH] = {};
		GetCurrentDirectoryW(MAX_PATH, dir);
		std::wstring ini = std::wstring(dir) + L"\\Data\\Config\\config.ini";

		int count = GetPrivateProfileIntW(L"Stress", L"Count", 1000, ini.c_str());
		int variety = GetPrivateProfileIntW(L"Stress", L"Textures", 1, ini.c_str());
		bool atlas = GetPrivateProfileIntW(L"Stress", L"Atlas", 0, ini.c_str()) != 0;
		int pages = GetPrivateProfileIntW(L"Stress", L"AtlasPages", 1, ini.c_str());

		// Textures=1: 전부 같은 텍스처 → 한 배치(배칭/인스턴싱 최적).
		// Textures>1: 서로 다른 텍스처를 번갈아 → 배치가 끊겨 드로우콜↑.
		// Atlas=1: 5종을 아틀라스 서브렉트로 합침. AtlasPages=K 면 K장의 페이지(별도 SRV)에
		//          스프라이트를 랜덤 분산 → 그리기 순서 제약(알파 정렬 불가)을 모사한 realistic 부하.
		const char* plainTex[] = { "Block", "Spike", "Portal_0", "Pistol_0", "Idle_0" };
		const int kMaxVariety = 5;
		if (variety < 1) variety = 1;
		if (variety > kMaxVariety) variety = kMaxVariety;
		if (pages < 1) pages = 1;

		if (atlas)
			TextureManager::GetInstance()->BuildStressAtlas(pages);

		Manager::GetInstance()->RegisterRenderSystem<SpriteDrawSystem>();

		srand(1234); // 고정 시드 → 백엔드 간 동일 배치(공정 비교)

		for (int i = 0; i < count; ++i)
		{
			int sub = i % variety;
			int page = (pages > 1) ? (rand() % pages) : 0;
			std::string tex = atlas
				? ("AtlasP" + std::to_string(page) + "S" + std::to_string(sub))
				: std::string(plainTex[sub]);

			float x = (float)(rand() % SCREEN_WIDTH);
			float y = (float)(rand() % SCREEN_HEIGHT);

			Manager::GetInstance()->CreateEntity([&](Entity* e)
			{
				e->AddComponent<Position>(x, y);
				e->AddComponent<Sprite>(tex, 0.5f, 0.5f);
			});
		}
	}

	virtual void Update(float dt) override { Manager::GetInstance()->Tick(dt); }
	virtual void Render() override { Manager::GetInstance()->Render(); }
};
