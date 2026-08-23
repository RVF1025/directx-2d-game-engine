#include "SoundManager.h"

SoundManager::SoundManager()
{
	FMOD_System_Create(&m_pSystem);
	FMOD_System_Init(m_pSystem, 64, FMOD_INIT_NORMAL, NULL);
}

SoundManager::~SoundManager()
{
	FMOD_System_Close(m_pSystem);
	FMOD_System_Release(m_pSystem);
}

void SoundManager::CreateBG(int nCount, std::vector<std::string>& sounds)
{
	m_nBGCount = nCount;
	m_ppBG = new FMOD_SOUND*[nCount];
	m_ppBGChannel = new FMOD_CHANNEL*[nCount];

	for (int i = 0; i < nCount; i++)
		FMOD_System_CreateSound(m_pSystem, sounds[i].c_str(), FMOD_LOOP_NORMAL, 0, &m_ppBG[i]);
}



void SoundManager::CreateEF(int nCount, std::vector<std::string>& sounds)
{
	m_nEFCount = nCount;
	m_ppEF = new FMOD_SOUND*[nCount];
	m_ppEFChannel = new FMOD_CHANNEL*[nCount];

	for (int i = 0; i < nCount; i++)
		FMOD_System_CreateSound(m_pSystem, sounds[i].c_str(), FMOD_DEFAULT, 0, &m_ppEF[i]);
}

void SoundManager::PlayEF(int nIndex, float volume)
{
	if (nIndex < m_nEFCount)
	{
		FMOD_System_PlaySound(m_pSystem, FMOD_CHANNEL_FREE, m_ppEF[nIndex], 0, &m_ppEFChannel[nIndex]);
		if (volume > 0.f)
		{
			FMOD_Channel_SetVolume(m_ppEFChannel[nIndex], volume);
		}
	}
}

void SoundManager::PlayBG(int nIndex, float volume)
{
	if (nIndex < m_nBGCount)
	{
		FMOD_System_PlaySound(m_pSystem, FMOD_CHANNEL_FREE, m_ppBG[nIndex], 0, &m_ppBGChannel[nIndex]);
		if (volume > 0.f)
		{
			FMOD_Channel_SetVolume(m_ppBGChannel[nIndex], volume);
		}
	}
}

void SoundManager::StopBG(int nIndex)
{
	if (nIndex < m_nBGCount)
	{
		FMOD_Channel_Stop(m_ppBGChannel[nIndex]);
	}
}

void SoundManager::SetBGVolume(int nIndex, float volume)
{
	if (nIndex < m_nBGCount)
	{
		FMOD_Channel_SetVolume(m_ppBGChannel[nIndex], volume);
	}
}

void SoundManager::SetEFVolume(int nIndex, float volume)
{
	if (nIndex < m_nEFCount)
	{
		FMOD_Channel_SetVolume(m_ppEFChannel[nIndex], volume);
	}
}


void SoundManager::ReleaseSound()
{
	int i;

	delete[] m_ppBGChannel;

	for (i = 0; i < m_nBGCount; i++)
		FMOD_Sound_Release(m_ppBG[i]);
	delete[] m_ppBG;
	for (i = 0; i < m_nEFCount; i++)
		FMOD_Sound_Release(m_ppEF[i]);
	delete[] m_ppEF;
}

void SoundManager::Update()
{
	if (m_pSystem)
		FMOD_System_Update(m_pSystem);
}
