#pragma once
#include "Definitions.h"
#include "IRenderer.h"
#include <map>

class TextureManager : public Singleton<TextureManager>
{
public:
	TextureManager();
	~TextureManager();

public:
	void Init();
	void Release();

	TextureHandle* GetTexture(std::string filename, int idx = -1);
	int GetMaxIndex(std::string filename);

	// 벤치마크용: atlas.png 를 pages 장의 아틀라스 페이지(각각 별도 SRV)로 만들고,
	// 각 페이지의 5개 서브렉트를 "AtlasP{page}S{sub}" 이름으로 등록.
	// 페이지 수가 많을수록 SRV 전환이 늘어 배치/인스턴싱 병합이 realistic 하게 제한된다.
	void BuildStressAtlas(int pages = 1);

	FontHandle* GetSmallFont() { return &m_SmallFont; }
	FontHandle* GetLargeFont() { return &m_LargeFont; };

private:
	std::unordered_map<std::string, TextureHandle> m_Textures;
	std::unordered_map<std::string, TextureHandle> m_AtlasSubs;   // 서브렉트 핸들(SRV 비소유)
	std::vector<TextureHandle> m_AtlasPageOwners;                 // 추가 아틀라스 페이지 SRV(소유)
	std::unordered_map<std::string, int> m_MaxIndex;

	FontHandle m_SmallFont;
	FontHandle m_LargeFont;
};

