#pragma once
#include "SoundManager.h"

class SoundSystem : public System,
	public EventSubscriber<PlayBGM>,
	public EventSubscriber<PlaySFX>,
	public EventSubscriber<PlayerDamaged>,
	public EventSubscriber<PortalCollided>
{
public:
	virtual void Init(Manager& manager) override
	{
		manager.SubscribeEvent<SoundSystem, PlayBGM>();
		manager.SubscribeEvent<SoundSystem, PlaySFX>();
		manager.SubscribeEvent <SoundSystem, PlayerDamaged>();
		manager.SubscribeEvent <SoundSystem, PortalCollided>();

		std::vector<std::string> bg;
		bg.push_back("Resources/Sound/Background.mp3");
		SoundManager::GetInstance()->CreateBG(static_cast<int>(bg.size()), bg);
		SoundManager::GetInstance()->PlayBG(0, 0.2f);
		m_bisBGMPlaying = true;

		std::vector<std::string> ef;
		ef.push_back("Resources/Sound/Jump.mp3");
		ef.push_back("Resources/Sound/Spin.mp3");
		ef.push_back("Resources/Sound/Death.mp3");
		ef.push_back("Resources/Sound/Portal.mp3");
		SoundManager::GetInstance()->CreateEF(static_cast<int>(ef.size()), ef);

	}
	virtual void Tick(Manager& manager, float deltatime) override
	{

	}

	virtual void Received(Manager& manager, PlayBGM* event) override
	{
		if (event->m_bPlay)
		{
			if(m_bisBGMPlaying == false)
				SoundManager::GetInstance()->PlayBG(event->m_iKind, 0.2f);
		}
		else
		{
			if (m_bisBGMPlaying == true)
				SoundManager::GetInstance()->StopBG(event->m_iKind);
		}
	}

	virtual void Received(Manager& manager, PlaySFX* event) override
	{
		SoundManager::GetInstance()->PlayEF(event->m_iKind, 0.2f);
	}

	virtual void Received(Manager& manager, PlayerDamaged* event) override
	{
		SoundManager::GetInstance()->PlayEF(2, 0.15f);
	}

	virtual void Received(Manager& manager, PortalCollided* event) override
	{
		SoundManager::GetInstance()->PlayEF(3, 0.2f);
	}

private:
	bool m_bisBGMPlaying;
};