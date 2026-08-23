#include "TextureManager.h"
#include "GraphicManager.h"

#include <filesystem>
namespace fs = std::filesystem;

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{
}

void TextureManager::Init()
{
	auto* renderer = GraphicManager::GetInstance()->GetRenderer();

	for (const auto& entry : fs::recursive_directory_iterator(fs::current_path() / "Resources"))
	{
		auto ext = entry.path().extension();

		if (entry.path().extension() == ".png" || entry.path().extension() == ".jpg")
		{
			std::string stem = entry.path().stem().string();

			m_Textures.insert(std::make_pair(stem, renderer->CreateTexture(entry.path().wstring())));
			m_MaxIndex[stem.substr(0, stem.find('_'))]++;
		}
		else if(entry.path().extension() == ".ttf" )
		{
			AddFontResourceExW(entry.path().c_str(), FR_PRIVATE, NULL);

			m_SmallFont = renderer->CreateFont(L"MyFont", 30);
			m_LargeFont = renderer->CreateFont(L"MyFont", 50);
		}
	}
}

void TextureManager::BuildStressAtlas(int pages)
{
	auto atlasIt = m_Textures.find("atlas");
	if (atlasIt == m_Textures.end())
		return; // Resources/Atlas/atlas.png 가 로드되지 않음

	if (pages < 1) pages = 1;
	if (!m_AtlasSubs.empty()) return; // 이미 구축됨

	auto* renderer = GraphicManager::GetInstance()->GetRenderer();
	const float AW = 256.f, AH = 64.f;

	struct Sub { int x, y, w, h; };
	// atlas.png 팩킹 좌표(오프라인 생성 시 기록한 값)
	Sub subs[5] = {
		{   0, 0, 50, 50 }, // Block
		{  50, 0, 50, 50 }, // Spike
		{ 100, 0, 64, 64 }, // Portal_0
		{ 164, 0, 36, 14 }, // Pistol_0
		{ 200, 0, 40, 40 }, // Idle_0
	};

	for (int p = 0; p < pages; ++p)
	{
		// 페이지 0 은 이미 로드된 "atlas" SRV 재사용, 나머지는 별도 SRV 로 추가 로드
		// (내용은 동일하지만 SRV 가 달라 드로우콜이 분리됨 = 아틀라스 페이지 K장 모델).
		void* srv;
		if (p == 0)
		{
			srv = atlasIt->second.texture;
		}
		else
		{
			TextureHandle owner = renderer->CreateTexture(L"Resources/Atlas/atlas.png");
			m_AtlasPageOwners.push_back(owner);
			srv = owner.texture;
		}

		for (int s = 0; s < 5; ++s)
		{
			TextureHandle h;
			h.texture = srv;
			h.width = subs[s].w;
			h.height = subs[s].h;
			h.srcX = subs[s].x;
			h.srcY = subs[s].y;
			h.u0 = subs[s].x / AW;
			h.v0 = subs[s].y / AH;
			h.u1 = (subs[s].x + subs[s].w) / AW;
			h.v1 = (subs[s].y + subs[s].h) / AH;
			h.isSub = true;
			m_AtlasSubs["AtlasP" + std::to_string(p) + "S" + std::to_string(s)] = h;
		}
	}
}

TextureHandle* TextureManager::GetTexture(std::string filename, int idx)
{
	// 아틀라스 서브핸들 우선 조회(벤치마크 경로)
	{
		auto sub = m_AtlasSubs.find(filename);
		if (sub != m_AtlasSubs.end())
			return &sub->second;
	}

	if (idx != -1)
	{
		auto found = m_Textures.find(filename + "_" + std::to_string(idx));
		if (found != m_Textures.end())
		{
			return &found->second;
		}
	}
	else
	{
		auto found = m_Textures.find(filename);
		if (found != m_Textures.end())
		{
			return &found->second;
		}
		else
		{
			found = m_Textures.find(filename + "_" + std::to_string(0));
			if (found != m_Textures.end())
			{
				return &found->second;
			}
		}
	}
	
	return nullptr;
}

int TextureManager::GetMaxIndex(std::string filename)
{
	auto found = m_MaxIndex.find(filename);
	if (found != m_MaxIndex.end())
	{
		return found->second - 1;
	}
	return 0;
}

void TextureManager::Release()
{
	auto* renderer = GraphicManager::GetInstance()->GetRenderer();

	for (auto pair : m_Textures)
	{
		renderer->ReleaseTexture(pair.second);
	}

	renderer->ReleaseFont(m_SmallFont);
	renderer->ReleaseFont(m_LargeFont);

	m_AtlasSubs.clear(); // 서브핸들은 SRV 비소유 → 개별 해제 금지
	for (auto& owner : m_AtlasPageOwners) // 추가 아틀라스 페이지 SRV 만 해제
		renderer->ReleaseTexture(owner);
	m_AtlasPageOwners.clear();
	m_Textures.clear();
	m_MaxIndex.clear();
}