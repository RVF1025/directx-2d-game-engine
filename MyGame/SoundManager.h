#pragma once

#include "Definitions.h"
#include "fmod.h"

#pragma comment(lib, "fmodex64_vc.lib")

class SoundManager : public Singleton<SoundManager>
{
private:
	FMOD_SYSTEM* m_pSystem;
	FMOD_SOUND** m_ppBG;
	FMOD_SOUND** m_ppEF;
	FMOD_CHANNEL** m_ppBGChannel;
	FMOD_CHANNEL** m_ppEFChannel;
	int m_nEFCount;
	int m_nBGCount;

public:
	SoundManager();
	~SoundManager();

	void CreateEF(int nCount, std::vector<std::string>& sounds);
	void CreateBG(int nCount, std::vector<std::string>& sounds);
	void PlayEF(int nIndex, float volume = 0.f);
	void PlayBG(int nIndex, float volume = 0.f);
	void StopBG(int nIndex);
	void SetBGVolume(int nIndex, float volume);
	void SetEFVolume(int nIndex, float volume);
	void ReleaseSound();
	void Update();

};

